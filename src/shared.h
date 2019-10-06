#pragma once

#include <stdbool.h>

typedef void(*pollfd_callback)(int fd, unsigned revents, void* data);
void add_poll_handler(int fd, unsigned events, void* data,
		pollfd_callback callback);

void schedule_redraw();


// mpd
struct mpd;

struct mpd* mpd_create();
void mpd_destroy(struct mpd*);

// Returns an 'artist - title' description of the current song.
// Returns NULL if there is no current song (mpd is in stopped state).
const char* mpd_get_song(struct mpd*);

// Returns the current mpd state:
// 1: stop, 2: play, 3: pause
int mpd_get_state(struct mpd*);


// volume
struct volume;

struct volume* volume_create();
void volume_destroy(struct volume*);
unsigned volume_get(struct volume*);


// notes
struct notes;

struct notes* notes_create();
void notes_destroy(struct notes*);
unsigned notes_get(struct notes*, const char*[static 64]);


// TODO: implement for laptop
// unsigned get_brightness();

// look into https://github.com/aravind/libacpi
// struct battery_status {
// 	unsigned percent;
// 	unsigned prediction; // in minutes
// 	bool charging;
// };
//
// struct battery_status get_battery_status();
