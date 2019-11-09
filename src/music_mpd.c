#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <poll.h>
#include <mpd/client.h>
#include <mainloop.h>
#include "shared.h"
#include "music.h"
#include "display.h"
#include "banner.h"

struct mod_music {
	struct display* dpy;
	struct mpd_connection* connection;
	struct ml_io* io;
	char songbuf[256]; // "artist - title"
	enum music_state state;
	bool idle;
};

static bool mpd_check_error(struct mod_music* mpd) {
	enum mpd_error error = mpd_connection_get_error(mpd->connection);
	if(error != MPD_ERROR_SUCCESS) {
		const char* msg = mpd_connection_get_error_message(mpd->connection);
		printf("mpd connection error: '%s' (%d)\n", msg, error);
		return true;
	} else {
		return false;
	}
}

static void mpd_fill(struct mod_music* mpd) {
	// song
	struct mpd_song* song = mpd_run_current_song(mpd->connection);
	if(!song) {
		// TODO: this always returns a song, even when stopped...
		// rather use the state
		mpd_check_error(mpd);
		mpd->songbuf[0] = '\0';
		return;
	}

	const char* artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
	const char* title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	if(!artist) {
		artist = "<unknown>";
	}
	if(!title) {
		title = "<unknown>";
	}
	snprintf(mpd->songbuf, sizeof(mpd->songbuf), "%s - %s", artist, title);

	struct mpd_status* status = mpd_run_status(mpd->connection);
	mpd->state = (enum music_state) mpd_status_get_state(status);
	mpd_status_free(status);
}

static void mpd_read(struct ml_io* io, unsigned revents) {
	(void) revents;
	struct mod_music* mpd = (struct mod_music*) ml_io_get_data(io);

	if(mpd_run_noidle(mpd->connection) == MPD_IDLE_PLAYER) {
		mpd_fill(mpd);
		display_redraw(mpd->dpy, banner_music);
	}

	mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER);
}

struct mod_music* mod_music_create(struct display* dpy) {
	struct mod_music* mpd = calloc(1, sizeof(*mpd));
	mpd->dpy = dpy;
	mpd->connection = mpd_connection_new(NULL, 0, 0);
	if(mpd_check_error(mpd)) {
		goto err;
	}

	mpd_fill(mpd);

	// add to poll list
	mpd->io = ml_io_new(dui_mainloop(), mpd_connection_get_fd(mpd->connection),
		POLLIN, mpd_read);
	ml_io_set_data(mpd->io, mpd);

	// start initial idling
	mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER);
	mpd->idle = true;

	return mpd;

err:
	mod_music_destroy(mpd);
	return NULL;
}

void mod_music_destroy(struct mod_music* mpd) {
	if(mpd->io) {
		ml_io_destroy(mpd->io);
	}
	if(mpd->idle) {
		mpd_run_noidle(mpd->connection);
	}

	mpd_connection_free(mpd->connection);
	free(mpd);
}

const char* mod_music_get_song(struct mod_music* mpd) {
	if(mpd->songbuf[0] == '\0') {
		return NULL;
	}

	return mpd->songbuf;
}

enum music_state mod_music_get_state(struct mod_music* mpd) {
	return mpd->state;
}

void mod_music_next(struct mod_music* mpd) {
	if(mpd->idle) mpd_run_noidle(mpd->connection);
		mpd_run_next(mpd->connection);
		mpd_fill(mpd);
	if(mpd->idle) mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER); \
	display_show_banner(mpd->dpy, banner_music);
}

void mod_music_prev(struct mod_music* mpd) {
	if(mpd->idle) mpd_run_noidle(mpd->connection);
		mpd_run_previous(mpd->connection);
		mpd_fill(mpd);
	if(mpd->idle) mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER); \
	display_show_banner(mpd->dpy, banner_music);
}

void mod_music_toggle(struct mod_music* mpd) {
	if(mpd->idle) mpd_run_noidle(mpd->connection);
		mpd_run_toggle_pause(mpd->connection);
		mpd_fill(mpd);
	if(mpd->idle) mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER); \
	display_show_banner(mpd->dpy, banner_music);
}
