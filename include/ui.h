#pragma once

#include "banner.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;
struct modules;

// draw
struct ui;

struct ui* ui_create(struct modules*);
void ui_destroy(struct ui*);

// Draws the ui on the given cairo surface, with the given context for it.
// The surface must have size (width, height).
// If (banner == banner_none) will draw the dashboard, otherwise the
// specified banner type.
void ui_draw(struct ui*, cairo_surface_t*, cairo_t*,
		unsigned width, unsigned height, enum banner);

// Passes the given pressed key to the ui.
// The key is a linux key code.
void ui_key(struct ui*, unsigned);

