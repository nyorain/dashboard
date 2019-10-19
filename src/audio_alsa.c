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
#include "audio.h"
#include "display.h"
#include "banner.h"

struct mod_audio {
	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
	struct display* dpy;
};

static int elem_callback(snd_mixer_elem_t* elem, unsigned int mask){
	(void) mask;
	(void) elem;
	struct mod_audio* mod = (struct mod_audio*) snd_mixer_elem_get_callback_private(elem);
	display_redraw(mod->dpy, banner_none);
	display_show_banner(mod->dpy, banner_volume);
	return 0;
}

static void volume_poll(int fd, unsigned revents, void* data) {
	(void) fd;
	(void) revents;
	struct mod_audio* mod = (struct mod_audio*) data;
	snd_mixer_handle_events(mod->handle);
}

struct mod_audio* mod_audio_create(struct display* dpy) {
	static const char* card = "default";
	static const unsigned smixer_level = 0;

	int err;
	struct mod_audio* mod = calloc(1, sizeof(*mod));
	mod->dpy = dpy;

	snd_mixer_selem_id_t* sid;
	snd_mixer_selem_id_alloca(&sid);

	if((err = snd_mixer_open(&mod->handle, 0)) < 0) {
		printf("Mixer %s open error: %s", card, snd_strerror(err));
		goto err;
	}

	if(smixer_level == 0 && (err = snd_mixer_attach(mod->handle, card)) < 0) {
		printf("Mixer attach %s error: %s", card, snd_strerror(err));
		goto err;
	}
	if((err = snd_mixer_selem_register(mod->handle, NULL, NULL)) < 0) {
		printf("Mixer register error: %s", snd_strerror(err));
		goto err;
	}
	err = snd_mixer_load(mod->handle);
	if (err < 0) {
		printf("Mixer %s load error: %s", card, snd_strerror(err));
		goto err;
	}

	snd_mixer_elem_t* elem;
	for(elem = snd_mixer_first_elem(mod->handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if(!snd_mixer_selem_is_active(elem)) {
			continue;
		}

		// NOTE: this is probably cheated, do this properly as soon
		// as it fails on any machine for the first time.
		const char* name = snd_mixer_selem_id_get_name(sid);
		if(!strcmp(name, "Master")) {
			mod->elem = elem;
			break;
		}
	}

	if(!mod->elem) {
		printf("Couldn't find 'Master' mixer element");
		goto err;
	}

	snd_mixer_elem_set_callback_private(mod->elem, mod);
	snd_mixer_elem_set_callback(mod->elem, elem_callback);

	int count = snd_mixer_poll_descriptors_count(mod->handle);
	struct pollfd* pfds = calloc(count, sizeof(*pfds));
	assert(snd_mixer_poll_descriptors(mod->handle, pfds, count) == count);
	for(int i = 0; i < count; ++i) {
		add_poll_handler(pfds[i].fd, pfds[i].events, mod, volume_poll);
	}

	return mod;

err:
	mod_audio_destroy(mod);
	return NULL;
}

void mod_audio_destroy(struct mod_audio* volume) {
	if(volume->handle) {
		snd_mixer_close(volume->handle);
	}
	free(volume);
}

unsigned mod_audio_get(struct mod_audio* mod) {
	assert(mod && mod->elem);
	assert(snd_mixer_selem_has_playback_volume(mod->elem));

	long pvol, pmin, pmax;
	snd_mixer_selem_get_playback_volume_range(mod->elem, &pmin, &pmax);
	snd_mixer_selem_get_playback_volume(mod->elem, 0, &pvol);

	return round(100 * (pvol - pmin) / (double)(pmax - pmin));
}

bool mod_audio_get_muted(struct mod_audio* mod) {
	if(snd_mixer_selem_has_common_switch(mod->elem) &&
			!snd_mixer_selem_has_playback_switch(mod->elem)) {
		printf("alsa get muted: elem has no switch\n");
		return false;
	}

	int muted;
	snd_mixer_selem_get_playback_switch(mod->elem, 0, &muted);
	return muted == 0;
}
