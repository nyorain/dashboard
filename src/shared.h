#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

struct modules;

enum banner {
	banner_none,
	banner_volume,
	banner_brightness,
	banner_battery,
	banner_music,
};

// size of the banner
static const unsigned banner_width = 400;
static const unsigned banner_height = 60;
// size of the dashboard
static const unsigned start_width = 800;
static const unsigned start_height = 500;

typedef void(*pollfd_callback)(int fd, unsigned revents, void* data);
void add_poll_handler(int fd, unsigned events, void* data,
		pollfd_callback callback);

struct inotify_event; // sys/inotify.h
typedef void(*inotify_callback)(const struct inotify_event*, void* data);
int add_inotify_watch(const char* pathname, uint32_t mask,
		void* data, inotify_callback callback);
void rm_inotify_watch(int wd);
struct display* display_get(void);

// Like bsd's strlcpy (safer alternative to strncpy) but copies
// a number of utf8 characters. Note that this means that dst
// has to be at least maxncpy * 4 (or times 6 for weird chars) long.
char* utf8_strlcpy(char* dst, const char* src, size_t maxncpy);

// Returns the length of the utf-8 encoded character in bytes.
unsigned utf8_length(const char* src);


// draw
struct ui;

struct ui* ui_create(struct modules*);
void ui_destroy(struct ui*);

void ui_draw_dashboard(struct ui*, cairo_surface_t*, cairo_t*);
void ui_draw_banner(struct ui*, cairo_surface_t*, cairo_t*, enum banner);
void ui_key(struct ui*, unsigned); // linux key codes

// display
struct display;
struct display_impl {
	void (*destroy)(struct display*);
	void (*toggle_dashboard)(struct display*);
	void (*redraw_dashboard)(struct display*);
	void (*show_banner)(struct display*, enum banner);
};

struct display {
	const struct display_impl* impl;
};

// These functions returns NULL in case the backend isn't available.
struct display* display_create_wl(struct ui*);
struct display* display_create_x11(struct ui*);
void display_destroy(struct display*);

// Activates a banner/notification of the given type.
// Will automatically hide after some time.
// Has no effect if the dashboard is currently mapped.
// Will also unmap all old banners.
void display_show_banner(struct display*, enum banner);

// Maps the dashboard. Will automatically unmap the currnet banner.
void display_toggle_dashboard(struct display*);

// Redraws the contents of the dashboard.
void display_redraw_dashboard(struct display*);


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

// Play the next/prev song in the current list.
// Will display a banner with the new song title.
void mpd_next(struct mpd*);
void mpd_prev(struct mpd*);

// Toggles whether mpd is playing. Will display a banner.
void mpd_toggle(struct mpd*);


// volume
struct volume;

struct volume* volume_create(void);
void volume_destroy(struct volume*);
unsigned volume_get(struct volume*);
bool volume_get_muted(struct volume*);


// notes
struct notes;

struct notes* notes_create(void);
void notes_destroy(struct notes*);
const char** notes_get(struct notes*, unsigned* count);

// brightness
struct brightness;

struct brightness* brightness_create(void);
void brightness_destroy(struct brightness*);

// Returns the current brightness in percent.
// A return code <0 means that the brightness couldn't be read.
int brightness_get(struct brightness*);


// battery & power module
struct battery;
struct battery* battery_create(void);
void battery_destroy(struct battery*);

struct battery_status {
	unsigned percent;
	// unsigned prediction; // in minutes; TODO
	bool charging;
	float wattage;
};

struct battery_status battery_get(struct battery* battery);


// playerctl
struct playerctl;
struct playerctl* playerctl_create(void);
void playerctl_destroy(struct playerctl*);

// Returns an 'artist - title' description of the current song.
// Returns NULL if there is no current song (player is in stopped state).
const char* playerctl_get_song(struct playerctl*);

// Returns the current player state:
// 1: stop, 2: play, 3: pause
int playerctl_get_state(struct playerctl*);

// Play the next/prev song in the current list.
// Will display a banner with the new song title.
void playerctl_next(struct playerctl*);
void playerctl_prev(struct playerctl*);

// Toggles whether player is playing. Will display a banner.
void playerctl_toggle(struct playerctl*);

struct modules {
	struct mpd* mpd;
	struct volume* volume;
	struct notes* notes;
	struct brightness* brightness;
	struct battery* battery;
	struct playerctl* playerctl;
};
