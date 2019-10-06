#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void(*pollfd_callback)(int fd, unsigned revents, void* data);
void add_poll_handler(int fd, unsigned events, void* data,
		pollfd_callback callback);

struct inotify_event; // sys/inotify.h
typedef void(*inotify_callback)(const struct inotify_event*, void* data);
int add_inotify_watch(const char* pathname, uint32_t mask,
		void* data, inotify_callback callback);
void rm_inotify_watch(int wd);

void schedule_redraw(void);


// mpd
struct mpd;

struct mpd* mpd_create(void);
void mpd_destroy(struct mpd*);

// Returns an 'artist - title' description of the current song.
// Returns NULL if there is no current song (mpd is in stopped state).
const char* mpd_get_song(struct mpd*);

// Returns the current mpd state:
// 1: stop, 2: play, 3: pause
int mpd_get_state(struct mpd*);


// volume
struct volume;

struct volume* volume_create(void);
void volume_destroy(struct volume*);
unsigned volume_get(struct volume*);


// notes
struct notes;

struct notes* notes_create(void);
void notes_destroy(struct notes*);
unsigned notes_get(struct notes*, const char*[static 64]);

// brightness
struct brightness;

struct brightness* brightness_create(void);
void brightness_destroy(struct brightness*);

// Returns the current brightness in percent.
// A return code <0 means that the brightness couldn't be read.
int get_brightness(struct brightness*);

// TODO: battery/energy/power status for laptop
// look into https://github.com/aravind/libacpi
// struct battery_status {
// 	unsigned percent;
// 	unsigned prediction; // in minutes
// 	bool charging;
// };
//
// struct battery_status get_battery_status();
