#define _POSIX_C_SOURCE 200809L

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

#include <mainloop.h>
#include "shared.h"
#include "display.h"
#include "music.h"
#include "audio.h"
#include "brightness.h"
#include "power.h"
#include "notes.h"
#include "ui.h"

struct context {
	int fifo;
	struct mainloop* mainloop;

	struct ui* ui;
	struct display* display;
	struct modules modules;
	bool run;
} ctx = {0};

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
	} else if(strcmp(msg, "audio cycle-output") == 0) {
		if(ctx.modules.audio) mod_audio_cycle_output(ctx.modules.audio);
	} else if(strcmp(msg, "audio up") == 0) {
		if(ctx.modules.audio) mod_audio_add(ctx.modules.audio, 5);
	} else if(strcmp(msg, "audio down") == 0) {
		if(ctx.modules.audio) mod_audio_add(ctx.modules.audio, -5);
	} else if(strcmp(msg, "exit") == 0) {
		ctx.run = false;
	} else {
		printf("Unknown message: '%s'\n", msg);
	}
}

static void fifo_read(struct ml_io* io, enum ml_io_flags revents) {
	(void) revents;

	int fd = ml_io_get_fd(io);
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

struct mainloop* dui_mainloop(void) {
	return ctx.mainloop;
}

int main() {
	ctx.mainloop = mainloop_new();
	if(!ctx.mainloop) {
		return EXIT_FAILURE;
	}

	// init daemon fifo
	// EEXIST simply means that the file already exists. We just
	// assume that it is a pipe from a previous process of this
	// program.
	const char* fifo_path = "/tmp/.dui-pipe";
	int ret = mkfifo(fifo_path, 0666);
	if(ret != 0) {
		if(errno == EEXIST) {
			printf("/tmp/.dui-pipe already exists. Another instance running?\n"
				"In case a previous instance crashed, remove that file manually\n");
			return 2;
		}

		printf("mkfifo failed: %s (%d)\n", strerror(errno), errno);
		return 3;
	}

	// we use O_RDWR here since that won't close our side of the pipe
	// when a writer closes their side. We never write to it though.
	// See the excellent post on https://stackoverflow.com/questions/15055065
	// for details. Linux specific
	ctx.fifo = open(fifo_path, O_NONBLOCK | O_CLOEXEC | O_RDWR);
	if(ctx.fifo < 0) {
		printf("open on fifo failed: %s (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}

	ml_io_new(ctx.mainloop, ctx.fifo, POLLIN, fifo_read);

	// try to create all modules
	ctx.ui = ui_create(&ctx.modules);
	if(!ctx.ui) {
		return EXIT_FAILURE;
	}
	ctx.display = display_create(ctx.ui);
	if(!ctx.display) {
		return EXIT_FAILURE;
	}

	ctx.modules.music = mod_music_create(ctx.display);
	ctx.modules.audio = mod_audio_create(ctx.display);
	ctx.modules.notes = mod_notes_create(ctx.display);
	ctx.modules.brightness = mod_brightness_create(ctx.display);
	ctx.modules.power = mod_power_create(ctx.display);

	ctx.run = true;
	while(ctx.run) {
		mainloop_iterate(ctx.mainloop);
	}

	if(unlink(fifo_path) < 0) {
		printf("unlink failed: %s (%d)\n", strerror(errno), errno);
	}

	if(ctx.modules.power) mod_power_destroy(ctx.modules.power);
	if(ctx.modules.music) mod_music_destroy(ctx.modules.music);
	if(ctx.modules.audio) mod_audio_destroy(ctx.modules.audio);
	if(ctx.modules.notes) mod_notes_destroy(ctx.modules.notes);
	if(ctx.modules.brightness) mod_brightness_destroy(ctx.modules.brightness);
	display_destroy(ctx.display);
	ui_destroy(ctx.ui);
	mainloop_destroy(ctx.mainloop);
}
