#pragma once

struct display;

// Like mpd_state
enum music_state {
	music_state_none = 0,
	music_state_stopped = 1,
	music_state_playing = 2,
	music_state_paused = 3,
};

struct mod_music;

struct mod_music* mod_music_create(struct display*);
void mod_music_destroy(struct mod_music*);

// Returns an 'artist - title' description of the current song.
// Returns NULL if there is no current song (mpd is in stopped state).
const char* mod_music_get_song(struct mod_music*);

// Returns the current mpd state:
enum music_state mod_music_get_state(struct mod_music*);

// Play the next/prev song in the current list.
// Will display a banner with the new song title.
void mod_music_next(struct mod_music*);
void mod_music_prev(struct mod_music*);

// Toggles whether mpd is playing. Will display a banner.
void mod_music_toggle(struct mod_music*);
