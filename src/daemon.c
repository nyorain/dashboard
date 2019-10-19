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
#include "display.h"
#include "music.h"
#include "audio.h"
#include "brightness.h"
#include "power.h"
#include "notes.h"
#include "ui.h"

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

	struct ui* ui;
	struct display* display;
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
	if(strcmp(msg, "music next") == 0) {
		if(ctx.modules.music) mod_music_next(ctx.modules.music);
	} else if(strcmp(msg, "music prev") == 0) {
		if(ctx.modules.music) mod_music_prev(ctx.modules.music);
	} else if(strcmp(msg, "music toggle") == 0) {
		if(ctx.modules.music) mod_music_toggle(ctx.modules.music);
	} else if(strcmp(msg, "dashboard toggle") == 0) {
		display_toggle_dashboard(ctx.display);
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
	ctx.ui = ui_create(&ctx.modules);
	ctx.display = display_create(ctx.ui);
	ctx.modules.music = mod_music_create(ctx.display);
	ctx.modules.audio = mod_audio_create(ctx.display);
	ctx.modules.notes = mod_notes_create(ctx.display);
	ctx.modules.brightness = mod_brightness_create(ctx.display);
	ctx.modules.power = mod_power_create(ctx.display);

	// critical modules
	if(!ctx.display) {
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
	if(ctx.modules.power) mod_power_destroy(ctx.modules.power);
	if(ctx.modules.music) mod_music_destroy(ctx.modules.music);
	if(ctx.modules.audio) mod_audio_destroy(ctx.modules.audio);
	if(ctx.modules.notes) mod_notes_destroy(ctx.modules.notes);
	if(ctx.modules.brightness) mod_brightness_destroy(ctx.modules.brightness);
	if(ctx.display) display_destroy(ctx.display);
	if(ctx.ui) ui_destroy(ctx.ui);
}
