#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/inotify.h>
#include "shared.h"
#include "brightness.h"
#include "banner.h"
#include "display.h"

#define BASE_PATH "/sys/class/backlight/intel_backlight/"
static const char* path_max = BASE_PATH "max_brightness";
static const char* path_current = BASE_PATH "actual_brightness";

struct mod_brightness {
	struct display* dpy;
	int wd; // inotify watcher
	int percent;
};

static int read_percent() {
	FILE* fdmax = fopen(path_max, "r");
	if(!fdmax) {
		return -1;
	}

	FILE* fdcurrent = fopen(path_current, "r");
	if(!fdcurrent) {
		fclose(fdmax);
		return -2;
	}

	char buf[32];
	fread(buf, 1, 32, fdmax);
	int max = atoi(buf);

	fread(buf, 1, 32, fdcurrent);
	int current = atoi(buf);

	fclose(fdmax);
	fclose(fdcurrent);
	return round(100 * current / (double)max);
}

static void callback(const struct inotify_event* ev, void* data) {
	(void) ev;
	struct mod_brightness* mod = (struct mod_brightness*) data;
	mod->percent = read_percent();
	display_redraw(mod->dpy, banner_none);
	display_show_banner(mod->dpy, banner_brightness);
}

struct mod_brightness* mod_brightness_create(struct display* dpy) {
	int p = read_percent();
	if(p < 0) {
		return NULL;
	}

	struct mod_brightness* mod = calloc(1, sizeof(*mod));
	mod->dpy = dpy;
	mod->percent = p;
	mod->wd = add_inotify_watch(path_current, IN_MODIFY, mod, callback);
	return mod;
}

void mod_brightness_destroy(struct mod_brightness* mod) {
	rm_inotify_watch(mod->wd);
	free(mod);
}

int mod_brightness_get(struct mod_brightness* mod) {
	return mod->percent;
}
