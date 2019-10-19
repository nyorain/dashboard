#pragma once

#include "banner.h"

struct ui;
struct display;

struct display_impl {
	void (*destroy)(struct display*);
	void (*toggle_dashboard)(struct display*);
	void (*redraw)(struct display*, enum banner);
	void (*show_banner)(struct display*, enum banner);
};

struct display {
	const struct display_impl* impl;
};

// Will automatically select a backend
struct display* display_create(struct ui*);
void display_destroy(struct display*);

// Activates a banner/notification of the given type.
// Will automatically hide after some time.
// Has no effect if the dashboard is currently mapped.
// Will also unmap all old banners.
void display_show_banner(struct display*, enum banner);

// Maps the dashboard. Will automatically unmap the current banner
// if there is any.
void display_toggle_dashboard(struct display*);

// Redraws the contents of the dashboard, if shown.
// If a banner is currently shown and it equals the passed banner,
// it will be redrawn.
void display_redraw(struct display*, enum banner);

// NOTE: shouldn't probably not be here...
// size of the banner
static const unsigned banner_width = 400;
static const unsigned banner_height = 60;
// size of the dashboard
static const unsigned start_width = 800;
static const unsigned start_height = 500;

