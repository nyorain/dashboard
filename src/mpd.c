#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/poll.h>
#include <mpd/client.h>
#include "shared.h"

struct mpd {
	struct mpd_connection* connection;
	char songbuf[64]; // "artist - title"
	enum mpd_state state;
	bool idle;
};

#define WRAP_NOIDLE(x) \
	if(mpd->idle) { \
		mpd_run_noidle(mpd->connection); \
	} \
	x \
	if(mpd->idle) { \
		mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER); \
	}

static bool mpd_check_error(struct mpd* mpd) {
	enum mpd_error error = mpd_connection_get_error(mpd->connection);
	if(error != MPD_ERROR_SUCCESS) {
		const char* msg = mpd_connection_get_error_message(mpd->connection);
		printf("mpd connection error: '%s' (%d)\n", msg, error);
		return true;
	} else {
		return false;
	}
}

static void mpd_fill(struct mpd* mpd) {
	// song
	struct mpd_song* song = mpd_run_current_song(mpd->connection);
	if(!song) {
		// TODO: this always returns a song, even when stopped...
		// rather user the state
		mpd_check_error(mpd);
		mpd->songbuf[0] = '\0';
		return;
	}

	const char* artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
	const char* title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	assert(artist && title);
	snprintf(mpd->songbuf, sizeof(mpd->songbuf), "%s - %s", artist, title);

	struct mpd_status* status = mpd_run_status(mpd->connection);
	mpd->state = mpd_status_get_state(status);
	mpd_status_free(status);
}

static void mpd_read(int fd, unsigned revents, void* data) {
	(void) fd;
	(void) revents;
	struct mpd* mpd = (struct mpd*) data;

	if(mpd_run_noidle(mpd->connection) == MPD_IDLE_PLAYER) {
		mpd_fill(mpd);
		display_redraw_dashboard(display_get());
	}

	mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER);
}

struct mpd* mpd_create() {
	struct mpd* mpd = calloc(1, sizeof(*mpd));
	mpd->connection = mpd_connection_new(NULL, 0, 0);
	if(mpd_check_error(mpd)) {
		goto err;
	}

	mpd_fill(mpd);

	// add to poll list
	add_poll_handler(mpd_connection_get_fd(mpd->connection), POLLIN,
		mpd, mpd_read);

	// start initial idling
	mpd_send_idle_mask(mpd->connection, MPD_IDLE_PLAYER);
	mpd->idle = true;

	return mpd;

err:
	mpd_destroy(mpd);
	return NULL;
}

void mpd_destroy(struct mpd* mpd) {
	if(mpd->idle) {
		mpd_run_noidle(mpd->connection);
	}

	mpd_connection_free(mpd->connection);
	free(mpd);
}

const char* mpd_get_song(struct mpd* mpd) {
	if(mpd->songbuf[0] == '\0') {
		return NULL;
	}

	return mpd->songbuf;
}

int mpd_get_state(struct mpd* mpd) {
	return (int) mpd->state;
}

void mpd_next(struct mpd* mpd) {
	WRAP_NOIDLE(
		mpd_run_next(mpd->connection);
		mpd_fill(mpd);
	);
	display_show_banner(display_get(), banner_music);
}

void mpd_prev(struct mpd* mpd) {
	WRAP_NOIDLE(
		mpd_run_previous(mpd->connection);
		mpd_fill(mpd);
	);
	display_show_banner(display_get(), banner_music);
}

void mpd_toggle(struct mpd* mpd) {
	WRAP_NOIDLE(
		mpd_run_toggle_pause(mpd->connection);
		mpd_fill(mpd);
	);
	display_show_banner(display_get(), banner_music);
}
