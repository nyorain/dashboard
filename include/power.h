#pragma once

#include <stdbool.h>

struct display;

struct mod_power;
struct mod_power* mod_power_create(struct display*);
void mod_power_destroy(struct mod_power*);

struct mod_power_status {
	unsigned percent;
	// unsigned prediction; // in minutes; TODO
	bool charging;
	float wattage;
};

struct mod_power_status mod_power_get(struct mod_power* battery);
