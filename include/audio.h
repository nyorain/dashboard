#pragma once

#include <stdbool.h>

struct display;
struct mod_audio;

struct mod_audio* mod_audio_create(struct display*);
void mod_audio_destroy(struct mod_audio*);

// Returns the current volume in percent
unsigned mod_audio_get(struct mod_audio*);

// Returns whether audio is currently muted.
bool mod_audio_get_muted(struct mod_audio*);

// Cycles to the next audio output.
void mod_audio_cycle_output(struct mod_audio*);

// Adds the given percent value to the current volume
void mod_audio_add(struct mod_audio*, int percent);
