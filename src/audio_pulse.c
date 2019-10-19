#include "audio.h"
#include <stdlib.h>
#include <pulse/pulseaudio.h>

// TODO: implement!

struct mod_audio {
	struct display* dpy;
};

struct mod_audio* mod_audio_create(struct display* dpy) {
	struct mod_audio* mod = calloc(1, sizeof(*mod));
	mod->dpy = dpy;
	return mod;
}

void mod_audio_destroy(struct mod_audio* mod) {
	free(mod);
}

unsigned mod_audio_get(struct mod_audio* m) { return 0; }
bool mod_audio_get_muted(struct mod_audio* m) { return false; }
void mod_audio_cycle_output(struct mod_audio* m) {}


