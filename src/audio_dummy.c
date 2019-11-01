#include "audio.h"
#include "shared.h"
#include <stdlib.h>

struct mod_audio* mod_audio_create(struct display* dpy) { return NULL; }
void mod_audio_destroy(struct mod_audio* m) {}
unsigned mod_audio_get(struct mod_audio* m) { DUI_DUMMY_IMPL; return 0; }
bool mod_audio_get_muted(struct mod_audio* m) { DUI_DUMMY_IMPL; return false; }
void mod_audio_cycle_output(struct mod_audio* m) { DUI_DUMMY_IMPL; }
void mod_audio_add(struct mod_audio* mod, int percent) { DUI_DUMMY_IMPL; }
