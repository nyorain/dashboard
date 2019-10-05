#pragma once

#include <stdbool.h>

// mpd
struct mpd;

struct mpd* mpd_create();
void mpd_destroy(struct mpd*);

// Prints an 'artist - title' description of the currently playing song
// into the given sized buffer.
// Returns false and doesn't print anything if there is no current song
bool mpd_get_song(struct mpd*, unsigned buf_size, char* buf);

// Returns whether mpd is currently playing
bool mpd_get_playing(struct mpd*);


// volume
struct volume;

struct volume* volume_create();
void volume_destroy(struct volume*);
unsigned volume_get(struct volume*);


// notes
struct notes;

struct notes* notes_create();
void notes_destroy(struct notes*);
const char** notes_get(struct notes*);


unsigned get_brightness();

// look into https://github.com/aravind/libacpi
struct battery_status {
	unsigned percent;
	unsigned prediction; // in minutes
	bool charging;
};

struct battery_status get_battery_status();
