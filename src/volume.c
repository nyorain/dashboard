#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "shared.h"

struct volume {
	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
};

static int elem_callback(snd_mixer_elem_t* elem, unsigned int mask){
	(void) mask;
	(void) elem;
	display_redraw_dashboard(display_get());
	display_show_banner(display_get(), banner_volume);
	return 0;
}

static void volume_poll(int fd, unsigned revents, void* data) {
	(void) fd;
	(void) revents;
	struct volume* volume = (struct volume*) data;
	snd_mixer_handle_events(volume->handle);
}

struct volume* volume_create() {
	static const char* card = "default";
	static const unsigned smixer_level = 0;

	int err;
	struct volume* volume = calloc(1, sizeof(*volume));

	snd_mixer_selem_id_t* sid;
	snd_mixer_selem_id_alloca(&sid);

	if((err = snd_mixer_open(&volume->handle, 0)) < 0) {
		printf("Mixer %s open error: %s", card, snd_strerror(err));
		goto err;
	}

	if(smixer_level == 0 && (err = snd_mixer_attach(volume->handle, card)) < 0) {
		printf("Mixer attach %s error: %s", card, snd_strerror(err));
		goto err;
	}
	if((err = snd_mixer_selem_register(volume->handle, NULL, NULL)) < 0) {
		printf("Mixer register error: %s", snd_strerror(err));
		goto err;
	}
	err = snd_mixer_load(volume->handle);
	if (err < 0) {
		printf("Mixer %s load error: %s", card, snd_strerror(err));
		goto err;
	}

	snd_mixer_elem_t* elem;
	for(elem = snd_mixer_first_elem(volume->handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if(!snd_mixer_selem_is_active(elem)) {
			continue;
		}

		// NOTE: this is probably cheated, do this properly as soon
		// as it fails on any machine for the first time.
		const char* name = snd_mixer_selem_id_get_name(sid);
		if(!strcmp(name, "Master")) {
			volume->elem = elem;
			break;
		}
	}

	if(!volume->elem) {
		printf("Couldn't find 'Master' mixer element");
		goto err;
	}

	snd_mixer_elem_set_callback(volume->elem, elem_callback);

	int count = snd_mixer_poll_descriptors_count(volume->handle);
	struct pollfd* pfds = calloc(count, sizeof(*pfds));
	assert(snd_mixer_poll_descriptors(volume->handle, pfds, count) == count);
	for(int i = 0; i < count; ++i) {
		add_poll_handler(pfds[i].fd, pfds[i].events, volume, volume_poll);
	}

	return volume;

err:
	volume_destroy(volume);
	return NULL;
}

void volume_destroy(struct volume* volume) {
	if(volume->handle) {
		snd_mixer_close(volume->handle);
	}
	free(volume);
}

unsigned volume_get(struct volume* volume) {
	assert(volume && volume->elem);
	assert(snd_mixer_selem_has_playback_volume(volume->elem));

	long pvol, pmin, pmax;
	snd_mixer_selem_get_playback_volume_range(volume->elem, &pmin, &pmax);
	snd_mixer_selem_get_playback_volume(volume->elem, 0, &pvol);

	return round(100 * (pvol - pmin) / (double)(pmax - pmin));
}

bool volume_get_muted(struct volume* volume) {
	if(snd_mixer_selem_has_common_switch(volume->elem) &&
			!snd_mixer_selem_has_playback_switch(volume->elem)) {
		printf("volume_get_muted: elem has no switch\n");
		return false;
	}

	int muted;
	snd_mixer_selem_get_playback_switch(volume->elem, 0, &muted);
	return muted == 0;
}
