#include "music.h"
#include <stdlib.h>

struct mod_music* mod_music_create(struct display* dpy) { return NULL; }
void mod_music_destroy(struct mod_music* m) {}
const char* mod_music_get_song(struct mod_music* m) { return NULL; }
enum music_state mod_music_get_state(struct mod_music* m) { return music_state_none; }
void mod_music_next(struct mod_music* m) {}
void mod_music_prev(struct mod_music* m) {}
void mod_music_toggle(struct mod_music* m) {}
