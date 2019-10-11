#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "shared.h"

struct poll_handler {
	pollfd_callback callback;
	void* data;
};

struct context {
	int fifo;

	unsigned poll_count;
	struct pollfd* pollfds;
	struct poll_handler* poll_handler;

	bool showing_dashboard;

	struct modules modules;
} ctx = {0};

// polls the given fds but in comparison to poll will not return
// when this program receives a (non-terminating) signal
static int poll_nosig(struct pollfd* fds, nfds_t nfds, int timeout) {
	while(true) {
		int ret = poll(fds, nfds, timeout);
		if(ret != -1 || errno != EINTR) {
			return ret;
		}
	}
}

// messages are simple strings
// they are always expected to end with a newline
static void handle_msg(char* msg, unsigned length) {
	assert(length > 0);
	char* newline = strchr(msg, '\n');
	if(!newline) {
		printf("Incomplete message: '%s'\n", msg);
		return;
	}

	*newline = '\0';
	printf("Command: %s\n", msg);
	if(strcmp(msg, "mpd next") == 0) {
		mpd_next(ctx.modules.mpd);
	} else if(strcmp(msg, "mpd prev") == 0) {
		mpd_prev(ctx.modules.mpd);
	} else if(strcmp(msg, "mpd toggle") == 0) {
		mpd_toggle(ctx.modules.mpd);
	} else if(strcmp(msg, "dashboard toggle") == 0) {
		display_toggle_dashboard(ctx.modules.display);
	} else {
		printf("Unknown message: '%s'\n", msg);
	}
}

static void fifo_read(int fd, unsigned revents, void* data) {
	(void) revents;
	(void) data;

	char buf[256];
	int ret = read(fd, buf, sizeof(buf) - 1);
	if(ret > 0) {
		buf[ret] = '\0';
		handle_msg(buf, ret);
	}

	while(ret > 0 && ret == sizeof(buf) - 1) {
		ret = read(fd, buf, sizeof(buf) - 1);
		if(ret > 0) {
			buf[ret] = '\0';
			handle_msg(buf, ret);
		}
	}

	if(ret < 0) {
		printf("fifo read failed: %s (%d)\n", strerror(errno), errno);
	}
}

void add_poll_handler(int fd, unsigned events, void* data,
		pollfd_callback callback) {
	unsigned c = ++ctx.poll_count;
	ctx.poll_handler = realloc(ctx.poll_handler, c * sizeof(*ctx.poll_handler));
	ctx.pollfds = realloc(ctx.pollfds, c * sizeof(*ctx.pollfds));

	--c;
	ctx.pollfds[c].fd = fd;
	ctx.pollfds[c].events = events;
	ctx.pollfds[c].revents = 0;

	ctx.poll_handler[c].callback = callback;
	ctx.poll_handler[c].data = data;
}

struct display* display_get() {
	return ctx.modules.display;
}

int main() {
	// init daemon fifo
	// EEXIST simply means that the file already exists. We just
	// assume that it is a pipe from a previous process of this
	// program.
	const char* fifo_path = "/tmp/.pipe-dashboard";
	int ret = mkfifo(fifo_path, 0666);
	if(ret != 0 && errno != EEXIST) {
		printf("mkfifo failed: %s (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}

	// we use O_RDWR here since that won't close our side of the pipe
	// when a writer closes their side. We never write to it though.
	// See the excellent post on https://stackoverflow.com/questions/15055065
	// for details
	ctx.fifo = open(fifo_path, O_NONBLOCK | O_CLOEXEC | O_RDWR);
	if(ctx.fifo < 0) {
		printf("open on fifo failed: %s (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}

	add_poll_handler(ctx.fifo, POLLIN, NULL, fifo_read);

	// try to create all modules
	ctx.modules.mpd = mpd_create();
	ctx.modules.volume = volume_create();
	ctx.modules.notes = notes_create();
	ctx.modules.brightness = brightness_create();
	ctx.modules.battery = battery_create();
	ctx.modules.display = display_create(&ctx.modules);

	// critical modules
	if(!ctx.modules.display) {
		return EXIT_FAILURE;
	}

	while(true) {
		int ret = poll_nosig(ctx.pollfds, ctx.poll_count, -1);
		if(ret == -1) {
			printf("poll failed: %s (%d)\n", strerror(errno), errno);
			continue; // break here?
		}

		for(unsigned i = 0u; i < ctx.poll_count; ++i) {
			if(ctx.pollfds[i].revents) {
				ctx.poll_handler[i].callback(ctx.pollfds[i].fd,
					ctx.pollfds[i].revents, ctx.poll_handler[i].data);
				ctx.pollfds[i].revents = 0;
			}
		}
	}

	// modules
	if(ctx.modules.display) display_destroy(ctx.modules.display);
	if(ctx.modules.battery) battery_destroy(ctx.modules.battery);
	if(ctx.modules.mpd) mpd_destroy(ctx.modules.mpd);
	if(ctx.modules.volume) volume_destroy(ctx.modules.volume);
	if(ctx.modules.notes) notes_destroy(ctx.modules.notes);
	if(ctx.modules.brightness) brightness_destroy(ctx.modules.brightness);
}
