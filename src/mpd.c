#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <mpd/client.h>

struct mpd {
	struct mpd_connection* connection;
};

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

struct mpd* mpd_create() {
	struct mpd* mpd = calloc(1, sizeof(mpd));
	mpd->connection = mpd_connection_new(NULL, 0, 0);
	if(!mpd_check_error(mpd)) {
		printf("Succesfully connected to mpd\n");
	}

	return mpd;
}

void mpd_destroy(struct mpd* mpd) {
	mpd_connection_free(mpd->connection);
	free(mpd);
}

bool mpd_get_song(struct mpd* mpd, unsigned buf_size, char* buf) {
	struct mpd_song* song = mpd_run_current_song(mpd->connection);
	if(!song) {
		mpd_check_error(mpd);
		return false;
	}

	const char* artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
	const char* title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	snprintf(buf, buf_size, "%s - %s", artist, title);
	return true;
}

bool mpd_get_playing(struct mpd* mpd) {
	struct mpd_status* status = mpd_run_status(mpd->connection);
	bool playing = (mpd_status_get_state(status) == MPD_STATE_PLAY);
	mpd_status_free(status);
	return playing;
}
