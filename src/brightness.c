#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/inotify.h>
#include "shared.h"

static const char* path_max = "/sys/class/backlight/intel_backlight/max_brightness";
static const char* path_current = "/sys/class/backlight/intel_backlight/actual_brightness";

struct brightness {
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
	struct brightness* brightness = (struct brightness*) data;
	brightness->percent = read_percent();
	printf("callback: %d\n", brightness->percent);
	schedule_redraw();
}

struct brightness* brightness_create(void) {
	int p = read_percent();
	if(p < 0) {
		return NULL;
	}

	struct brightness* brightness = calloc(1, sizeof(*brightness));
	brightness->percent = p;
	brightness->wd = add_inotify_watch(path_current, IN_MODIFY, brightness,
		callback);
	return brightness;
}

void brightness_destroy(struct brightness* brightness) {
	rm_inotify_watch(brightness->wd);
	free(brightness);
}

int get_brightness(struct brightness* b) {
	return b->percent;
}
