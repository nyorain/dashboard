#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

struct mainloop;
struct mainloop* dui_mainloop(void);

struct inotify_event; // sys/inotify.h
typedef void(*inotify_callback)(const struct inotify_event*, void* data);
int add_inotify_watch(const char* pathname, uint32_t mask,
		void* data, inotify_callback callback);
void rm_inotify_watch(int wd);

// Like bsd's strlcpy (safer alternative to strncpy) but copies
// a number of utf8 characters. Note that this means that dst
// has to be at least maxncpy * 4 (or times 6 for weird chars) long.
char* utf8_strlcpy(char* dst, const char* src, size_t maxncpy);

// Returns the length of the utf-8 encoded character in bytes.
unsigned utf8_length(const char* src);

struct modules {
	struct mod_audio* audio;
	struct mod_music* music;
	struct mod_notes* notes;
	struct mod_brightness* brightness;
	struct mod_power* power;
};

