#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <mainloop.h>
#include "shared.h"
#include "audio.h"
#include "display.h"
#include "banner.h"

struct mod_audio {
	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
	struct display* dpy;
	struct ml_custom* source;
};

static int elem_callback(snd_mixer_elem_t* elem, unsigned int mask){
	(void) mask;
	(void) elem;
	struct mod_audio* mod = (struct mod_audio*) snd_mixer_elem_get_callback_private(elem);
	display_redraw(mod->dpy, banner_none);
	display_show_banner(mod->dpy, banner_volume);
	return 0;
}

static unsigned source_query(struct ml_custom* c, struct pollfd* fds,
		unsigned n_fds, int* timeout) {
	struct mod_audio* mod = (struct mod_audio*) ml_custom_get_data(c);
	int count = snd_mixer_poll_descriptors_count(mod->handle);
	if(count < 0) {
		printf("snd_mixer_poll_descriptors_count: %d\n", count);
		return 0;
	}

	*timeout = -1;
	if(n_fds > 0) {
		unsigned uc = count;
		n_fds = uc < n_fds ? uc : n_fds;
		int count2 = snd_mixer_poll_descriptors(mod->handle, fds, n_fds);
		assert((unsigned) count2 == n_fds);
	}

	return count;
}

static void source_dispatch(struct ml_custom* c, struct pollfd* fds,
		unsigned n_fds) {
	struct mod_audio* mod = (struct mod_audio*) ml_custom_get_data(c);
	unsigned short revents;
	snd_mixer_poll_descriptors_revents(mod->handle, fds, n_fds,
		&revents);
	if(revents) {
		snd_mixer_handle_events(mod->handle);
	}
}

static const struct ml_custom_impl custom_impl = {
	.query = source_query,
	.dispatch = source_dispatch,
};

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

	mod->source = ml_custom_new(dui_mainloop(), &custom_impl);
	ml_custom_set_data(mod->source, mod);

	return mod;

err:
	mod_audio_destroy(mod);
	return NULL;
}

void mod_audio_destroy(struct mod_audio* volume) {
	if(volume->source) {
		ml_custom_destroy(volume->source);
	}
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

void mod_audio_cycle_output(struct mod_audio* mod) {
	printf("mod_audio_cycle_output: not implemented for alsa\n");
}

void mod_audio_add(struct mod_audio* mod, int percent) {
	// TODO: we can implement that
	printf("mod_audio_add: not implemented for alsa (yet)\n");
}
