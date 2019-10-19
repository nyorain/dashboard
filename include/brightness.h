#pragma once

struct display;
struct mod_brightness;

struct mod_brightness* mod_brightness_create(struct display*);
void mod_brightness_destroy(struct mod_brightness*);

// Returns the current brightness in percent.
// A return code <0 means that the brightness couldn't be read.
int mod_brightness_get(struct mod_brightness*);
