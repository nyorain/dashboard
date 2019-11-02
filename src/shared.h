#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

struct mainloop;
struct mainloop* dui_mainloop(void);
void dui_exit(void);

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

// Should be used in dummy implementations of modules (except create an destroy).
// Their create functions always return NULL and therefore calling a function
// with a dummy modules is a programming error.
#define DUI_DUMMY_IMPL assert(false && "[%s:%d] Dummy implementation called!")
