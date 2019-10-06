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

// TODO: there may be race conditions between mixer_callback (and volume_thread)
// and the volume_get etc read functions
// TODO: cleanup not done correctly. volume_thread never ends, it should
// be joined. Not that easy though, we would have to wake up snd_mixer_wait
// somehow? probably better to do this via one central poll instead
// of multiple threads i guess

struct volume {
	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
	pthread_t thread;
};

static void* volume_thread(void* data) {
	struct volume* volume = (struct volume*) data;
	while(true) {
		snd_mixer_wait(volume->handle, INT_MAX);
		snd_mixer_handle_events(volume->handle);
	}
	return NULL;
}

int elem_callback(snd_mixer_elem_t* elem, unsigned int mask){
	(void) mask;
	(void) elem;
	printf("elem callback!\n");
	schedule_redraw();
	return 0;
}

struct volume* volume_create() {
	static const char card[64] = "default";
	static const unsigned smixer_level = 0;

	int err;
	struct volume* volume = calloc(1, sizeof(volume));

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

	// init thread that waits for events
	err = pthread_create(&volume->thread, NULL, volume_thread, volume);

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
