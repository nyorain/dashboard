#pragma once

#include <stdbool.h>
#include <stdint.h>

struct modules;

typedef void(*pollfd_callback)(int fd, unsigned revents, void* data);
void add_poll_handler(int fd, unsigned events, void* data,
		pollfd_callback callback);

struct inotify_event; // sys/inotify.h
typedef void(*inotify_callback)(const struct inotify_event*, void* data);
int add_inotify_watch(const char* pathname, uint32_t mask,
		void* data, inotify_callback callback);
void rm_inotify_watch(int wd);
struct display* display_get(void);

// display
enum banner {
	banner_none,
	banner_volume,
	banner_brightness,
	banner_battery,
	banner_music,
};

struct display;

struct display* display_create(struct modules*);
void display_destroy(struct display*);

// Activates a banner/notification of the given type.
// Will automatically hide after some time.
// Has no effect if the dashboard is currently mapped.
// Will also unmap all old banners.
void display_show_banner(struct display*, enum banner);

// Maps the dashboard. Will automatically unmap the currnet banner.
void display_map_dashboard(struct display*);
void display_unmap_dashboard(struct display*);

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
unsigned notes_get(struct notes*, const char*[static 64]);

// brightness
struct brightness;

struct brightness* brightness_create(void);
void brightness_destroy(struct brightness*);

// Returns the current brightness in percent.
// A return code <0 means that the brightness couldn't be read.
int get_brightness(struct brightness*);

// look into https://github.com/aravind/libacpi
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


struct modules {
	struct display* display;
	struct mpd* mpd;
	struct volume* volume;
	struct notes* notes;
	struct brightness* brightness;
	struct battery* battery;
};
