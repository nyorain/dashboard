#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "shared.h"
#include "power.h"

// NOTE: we don't use inofity here since it wouldn't make a lot of
// sense, battery status is pretty much continously changing.
// EDIT: it would make sense actually to allow sending notifications.
// But it's not supported in sysfs which kinda makes sense.

#define BASE_PATH "/sys/class/power_supply/"

struct mod_power {
	// TODO: not used
	struct display* dpy;
	const char* battery_path;
	const char* ac_path;
};

static int ignore_directory_entry(struct dirent *de) {
    return !strcmp(de->d_name, ".") || !strcmp(de->d_name, "..");
}

static int read_int(FILE* file) {
    int n = -1;
    fscanf(file, "%d", &n);
    return n;
}

struct mod_power* mod_power_create(struct display* dpy) {
	DIR* d = opendir(BASE_PATH);
	if(!d) {
		printf("battery: opendir failed: %s (%d)\n", strerror(errno), errno);
		return NULL;
	}

	bool found = false;
    struct dirent *de;
	while((de = readdir(d))) {
	    if(ignore_directory_entry(de)) {
			continue;
		}

		found = true;
		// printf("battery: %s\n", de->d_name);
	}

	closedir(d);
	if(!found) {
		return NULL;
	}

	struct mod_power* mod = calloc(1, sizeof(*mod));
	mod->dpy = dpy;
	return mod;
}

void mod_power_destroy(struct mod_power* mod) {
	free(mod);
}

struct mod_power_status mod_power_get(struct mod_power* mod) {
	struct mod_power_status status = {0};

	FILE* fcharging = fopen(BASE_PATH "AC/online", "r");
	if(fcharging) {
		status.charging = read_int(fcharging);
		fclose(fcharging);
	} else {
		status.charging = false;
	}

	FILE* ffull = fopen(BASE_PATH "BAT0/energy_full", "r");
	FILE* fcurrent = fopen(BASE_PATH "BAT0/energy_now", "r");
	if(ffull && fcurrent) {
		int full = read_int(ffull);
		int current = read_int(fcurrent);
		status.percent = (int) round(100 * (current / (float) full));
		fclose(ffull);
		fclose(fcurrent);
	}

	FILE* fwattage = fopen(BASE_PATH "BAT0/power_now", "r");
	if(fwattage) {
		int wattage = read_int(fwattage);
		status.wattage = wattage / (1000.f * 1000.f);
		fclose(fwattage);
	} else {
		status.wattage = -1.f;
	}

	// TODO: using power_now and available
	// status.prediction = 332;
	return status;
}
