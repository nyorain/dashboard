#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <cairo/cairo.h>
#include <mainloop.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "display.h"
#include "ui.h"
#include "shared.h"
#include "pool-buffer.h"

struct display_wl {
	struct display base;
	struct ui* ui;
	struct wl_display* display;
	struct wl_registry* registry;

	struct wl_compositor* compositor;
	struct wl_shm* shm;
	struct zwlr_layer_shell_v1* layer_shell;

	bool redraw;
	struct wl_callback* frame_callback;
	struct wl_surface* surface;
	struct zwlr_layer_surface_v1* layer_surface;
	struct pool_buffer buffers[2];

	struct ml_custom* source;
	struct ml_timer* timer;
	bool reading_display;

	bool configured;
	bool dashboard;
	enum banner banner;
	unsigned width, height;
};

static void fd_prepare(struct ml_custom* c) {
	struct display_wl* dpy = (struct display_wl*) ml_custom_get_data(c);

	// this can happen if the previous poll failed
	if(dpy->reading_display) {
		wl_display_cancel_read(dpy->display);
		dpy->reading_display = false;
	}

	// wl_display_prepare_read returns -1 if the event queue wasn't empty
	// we simply dispatch the pending events
	while(wl_display_prepare_read(dpy->display) == -1) {
		wl_display_dispatch_pending(dpy->display);
	}

	dpy->reading_display = true;
	int ret = wl_display_flush(dpy->display);
	if(ret == -1) {
		// TODO: we should handle EAGAIN case, no more data can
		// be written in that case. We could poll the display for
		// POLLOUT
		printf("wl_display_flush: %s (%d)\n", strerror(errno), errno);
	}
}

static unsigned fd_query(struct ml_custom* c, struct pollfd* fds,
		unsigned n_fds, int* timeout) {
	struct display_wl* dpy = (struct display_wl*) ml_custom_get_data(c);
	if(n_fds > 0) {
		fds[0].fd = wl_display_get_fd(dpy->display);
		fds[0].events = POLLIN;
	}

	*timeout = -1;
	return 1;
}

static void fd_dispatch(struct ml_custom* c, struct pollfd* fds, unsigned n_fds) {
	(void) fds;
	(void) n_fds;

	struct display_wl* dpy = (struct display_wl*) ml_custom_get_data(c);
	int ret = wl_display_read_events(dpy->display);
	if(ret == -1) {
		printf("wl_display_read_events: %s (%d)\n", strerror(errno), errno);
		return;
	}

	dpy->reading_display = false;
	wl_display_dispatch_pending(dpy->display);
}

static const struct ml_custom_impl custom_impl = {
	.prepare = fd_prepare,
	.query = fd_query,
	.dispatch = fd_dispatch
};

static void destroy(struct display* base) {
	struct display_wl* dpy = (struct display_wl*) base;
	destroy_buffer(&dpy->buffers[0]);
	destroy_buffer(&dpy->buffers[1]);
	if(dpy->frame_callback) wl_callback_destroy(dpy->frame_callback);
	if(dpy->layer_surface) zwlr_layer_surface_v1_destroy(dpy->layer_surface);
	if(dpy->surface) wl_surface_destroy(dpy->surface);
	if(dpy->compositor) wl_compositor_destroy(dpy->compositor);
	if(dpy->shm) wl_shm_destroy(dpy->shm);
	if(dpy->layer_shell) zwlr_layer_shell_v1_destroy(dpy->layer_shell);
	if(dpy->registry) wl_registry_destroy(dpy->registry);
	if(dpy->display) wl_display_disconnect(dpy->display);
	if(dpy->source) ml_custom_destroy(dpy->source);
	free(dpy);
}

static void draw(struct display_wl* dpy);
static void frame_done(void* data, struct wl_callback* cb, uint32_t value) {
	struct display_wl* dpy = data;
	wl_callback_destroy(dpy->frame_callback);
	dpy->frame_callback = NULL;

	if(dpy->redraw) {
		dpy->redraw = false;
		draw(dpy);
	}
}

static const struct wl_callback_listener frame_callback_listener = {
	.done = frame_done,
};

static void draw(struct display_wl* dpy) {
	if((!dpy->dashboard && dpy->banner == banner_none) ||
			(dpy->width == 0 || dpy->height == 0)) {
		wl_surface_attach(dpy->surface, NULL, 0, 0);
	} else {
		struct pool_buffer* buf = get_next_buffer(dpy->shm, dpy->buffers,
			dpy->width, dpy->height);
		assert(buf);
		ui_draw(dpy->ui, buf->cairo, dpy->width, dpy->height,
			dpy->dashboard ? banner_none : dpy->banner);
		cairo_surface_flush(buf->surface);
		wl_surface_attach(dpy->surface, buf->buffer, 0, 0);
		dpy->frame_callback = wl_surface_frame(dpy->surface);
		wl_callback_add_listener(dpy->frame_callback, &frame_callback_listener, dpy);
	}

	wl_surface_damage(dpy->surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(dpy->surface);
}

static void refresh(struct display_wl* dpy) {
	if(dpy->frame_callback || !dpy->configured) {
		dpy->redraw = true;
		return;
	}

	draw(dpy);
}

static void toggle_dashboard(struct display* base) {
	struct display_wl* dpy = (struct display_wl*) base;
	dpy->dashboard = !dpy->dashboard;

	if(dpy->dashboard) {
		if(dpy->banner != banner_none) { // hide banner
			dpy->banner = banner_none;
			ml_timer_restart(dpy->timer, NULL); // disable
		}

		dpy->width = start_width;
		dpy->height = start_height;
		zwlr_layer_surface_v1_set_size(dpy->layer_surface,
			dpy->width, dpy->height);
		zwlr_layer_surface_v1_set_anchor(dpy->layer_surface, 0);
		zwlr_layer_surface_v1_set_margin(dpy->layer_surface, 0, 0, 0, 0);
		wl_surface_commit(dpy->surface);
	}

	refresh(dpy);
}

static void redraw(struct display* base, enum banner banner) {
	struct display_wl* dpy = (struct display_wl*) base;
	if(dpy->dashboard || (banner != banner_none && dpy->banner == banner)) {
		refresh(dpy);
	}
}

static void show_banner(struct display* base, enum banner banner) {
	struct display_wl* dpy = (struct display_wl*) base;
	// don't show any banners while the dashboard is active
	if(dpy->dashboard) {
		return;
	}

	dpy->banner = banner;
	refresh(dpy);

	// TODO: exclusive zone
	dpy->width = banner_width;
	dpy->height = banner_height;
	zwlr_layer_surface_v1_set_size(dpy->layer_surface,
		banner_width, banner_height);
	zwlr_layer_surface_v1_set_anchor(dpy->layer_surface,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_margin(dpy->layer_surface,
		0, banner_margin_x, banner_margin_y, 0);
	wl_surface_commit(dpy->surface);

	// set timeout on timer
	// this will automatically override previously queued timers
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += banner_time;
	ml_timer_restart(dpy->timer, &ts);
}

static const struct display_impl display_impl = {
	.destroy = destroy,
	.toggle_dashboard = toggle_dashboard,
	.redraw = redraw,
	.show_banner = show_banner,
};

static void timer_cb(struct ml_timer* timer, const struct timespec* time) {
	(void) time;
	struct display_wl* dpy = ml_timer_get_data(timer);
	assert(dpy->banner != banner_none);
	assert(!dpy->dashboard);
	dpy->banner = banner_none;
	refresh(dpy);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct display_wl* dpy = data;
	dpy->width = width;
	dpy->height = height;
	dpy->configured = true;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	refresh(dpy);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	// struct display_wl* dpy = data;
	printf("wayland display: layer_surface_closed. "
		"Not sure how to handle\n");
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

// TODO: correct version numbers
static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	(void) version;
	struct display_wl *dpy = data;
	if(strcmp(interface, wl_compositor_interface.name) == 0) {
		dpy->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if(strcmp(interface, wl_shm_interface.name) == 0) {
		dpy->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		dpy->layer_shell = wl_registry_bind(registry, name,
			&zwlr_layer_shell_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

struct display* display_create_wl(struct ui* ui) {
	struct wl_display* wld = wl_display_connect(NULL);
	if(!wld) {
		return NULL;
	}

	struct display_wl* dpy = calloc(1, sizeof(*dpy));
	dpy->base.impl = &display_impl;
	dpy->display = wld;
	dpy->ui = ui;

	dpy->registry = wl_display_get_registry(wld);
	wl_registry_add_listener(dpy->registry, &registry_listener, dpy);
	wl_display_roundtrip(dpy->display);

	const char* missing = NULL;
	if(!dpy->layer_shell) missing = "wlr_layer_shell";
	if(!dpy->shm) missing = "wl_shm";
	if(!dpy->compositor) missing = "wl_compositor";

	if(missing) {
		printf("Missing required Wayland interface '%s'\n", missing);
		destroy(&dpy->base);
		return NULL;
	}

	// sources & timers
	dpy->source = ml_custom_new(dui_mainloop(), &custom_impl);
	ml_custom_set_data(dpy->source, dpy);

	dpy->timer = ml_timer_new(dui_mainloop(), NULL, timer_cb);
	ml_timer_set_data(dpy->timer, dpy);

	// create surface
	dpy->surface = wl_compositor_create_surface(dpy->compositor);
	dpy->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		dpy->layer_shell, dpy->surface, NULL,
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "dui");
	zwlr_layer_surface_v1_add_listener(dpy->layer_surface,
		&layer_surface_listener, dpy);

	return &dpy->base;
}
