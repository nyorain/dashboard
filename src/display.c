#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <linux/input.h>
#include <sys/poll.h>
#include <sys/timerfd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#include "shared.h"

struct display {
	struct modules* modules;
	xcb_connection_t* connection;
	xcb_screen_t* screen;
	xcb_ewmh_connection_t ewmh;
	xcb_colormap_t colormap;
	xcb_window_t window;

	struct {
		xcb_atom_t wm_delete_window;
	} atoms;

	cairo_surface_t* surface;
	cairo_t* cr;

	enum banner banner; // whether a banner is active.
	bool dashboard; // dashboard currently mapped

	unsigned width, height;
	int timer;
};

static const unsigned start_width = 800;
static const unsigned start_height = 500;
static const unsigned banner_width = 300;
static const unsigned banner_height = 80;
static const unsigned banner_margin_x = 20;
static const unsigned banner_margin_y = 20;
static const unsigned banner_time = 2; // in seconds

// Simple macro that checks cookies returned by xcb *_checked calls
// useful when something triggers and xcb error
#define XCB_CHECK(x) {\
	xcb_void_cookie_t cookie = (x); \
	xcb_generic_error_t* gerr = xcb_request_check(ctx->connection, cookie); \
	if(gerr) { \
		printf("[%s:%d] xcb error code: %d\n", __FILE__, __LINE__, gerr->error_code); \
		free(gerr); \
	}}

static const char* mpd_state_symbol(int mpd_state) {
	switch(mpd_state) {
		case 1: return u8"";
		case 2: return u8"";
		case 3: return u8"";
		default: return "?";
	}
}

static const char* battery_symbol(struct battery_status status) {
	if(status.charging) {
		return u8"";
	} else if(status.percent > 95) {
		return u8"";
	} else if(status.percent > 65) {
		return u8"";
	} else if(status.percent > 35) {
		return u8"";
	} else if(status.percent > 5) {
		return u8"";
	} else {
		return u8"";
	}
}

static const char* banner_symbol(struct display* ctx) {
	switch(ctx->banner) {
		case banner_volume: return "";
		case banner_brightness: return "";
		case banner_battery: return battery_symbol(battery_get(ctx->modules->battery));
		case banner_music: return mpd_state_symbol(mpd_get_state(ctx->modules->mpd));
		default: return "?";
	}
}

static void draw_banner(struct display* ctx) {
	// background
	cairo_set_source_rgba(ctx->cr, 0.1, 0.1, 0.1, 0.8);
	cairo_set_operator(ctx->cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(ctx->cr);

	const char* sym = banner_symbol(ctx);
	cairo_select_font_face(ctx->cr, "FontAwesome",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(ctx->cr, 40.0);
	cairo_set_source_rgb(ctx->cr, 0.9, 0.9, 0.9);
	cairo_move_to(ctx->cr, 20.0, 55.0);
	cairo_show_text(ctx->cr, sym);

	// TODO: render information

	// finish
	cairo_surface_flush(ctx->surface);
	xcb_flush(ctx->connection);
}

static void send_expose_event(struct display* ctx) {
	// NOTE: not exactly sure if xcb is threadsafe and we can use
	// this at any time tbh... there is no XInitThreads for xcb,
	// iirc it is always threadsafe but that should be investigated
	xcb_expose_event_t event = {0};
	event.window = ctx->window;
	event.response_type = XCB_EXPOSE;
	event.x = 0;
	event.y = 0;
	event.width = ctx->width;
	event.height = ctx->height;
	event.count = 1;

	xcb_send_event(ctx->connection, 0, ctx->window, 0, (const char*) &event);
	xcb_flush(ctx->connection);
}

static void draw_dashboard(struct display* ctx) {
	// printf("drawing\n");
	char buf[256];

	// background
	cairo_set_source_rgba(ctx->cr, 0.1, 0.1, 0.1, 0.6);
	cairo_set_operator(ctx->cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(ctx->cr);

	// date & time
	time_t t = time(NULL);
	struct tm tm_info;
	assert(localtime_r(&t, &tm_info));

	strftime(buf, sizeof(buf), "%H:%M", &tm_info);
	cairo_select_font_face(ctx->cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(ctx->cr, 50.0);
	cairo_set_source_rgb(ctx->cr, 0.9, 0.9, 0.9);
	cairo_move_to(ctx->cr, 32.0, 70.0);
	cairo_show_text(ctx->cr, buf);

	strftime(buf, sizeof(buf), "%d.%m.%y", &tm_info);
	cairo_set_font_size(ctx->cr, 20.0);
	cairo_set_source_rgba(ctx->cr, 0.8, 0.8, 0.8, 0.8);
	cairo_move_to(ctx->cr, 32.0, 100.0);
	cairo_show_text(ctx->cr, buf);

	// music
	cairo_set_font_size(ctx->cr, 18.0);
	cairo_select_font_face(ctx->cr, "FontAwesome",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

	const char* sym;
	int mpdstate = mpd_get_state(ctx->modules->mpd);
	sym = mpd_state_symbol(mpdstate);

	const char* song = mpd_get_song(ctx->modules->mpd);
	if(!song || mpdstate == 1) {
		song = "-";
	}

	cairo_set_source_rgba(ctx->cr, 0.8, 0.8, 0.8, 0.8);
	cairo_move_to(ctx->cr, 32.0, 180.0);
	cairo_show_text(ctx->cr, sym);

	cairo_select_font_face(ctx->cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_move_to(ctx->cr, 60.0, 180.0);
	cairo_show_text(ctx->cr, song);

	// volume
	if(ctx->modules->volume) {
		cairo_move_to(ctx->cr, 32.0, 220.0);
		cairo_select_font_face(ctx->cr, "FontAwesome",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_show_text(ctx->cr, u8"");

		cairo_select_font_face(ctx->cr, "DejaVu Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(ctx->cr, 60.0, 220.0);

		if(volume_get_muted(ctx->modules->volume)) {
			snprintf(buf, sizeof(buf), "MUTE");
		} else {
			unsigned vol = volume_get(ctx->modules->volume);
			snprintf(buf, sizeof(buf), "%d%%", vol);
		}
		cairo_show_text(ctx->cr, buf);
	}

	// brightness
	if(ctx->modules->brightness) {
		int brightness = get_brightness(ctx->modules->brightness);
		if(brightness >= 0) {
			cairo_move_to(ctx->cr, 152.0, 220.0);
			cairo_select_font_face(ctx->cr, "FontAwesome",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_show_text(ctx->cr, u8"");

			cairo_select_font_face(ctx->cr, "DejaVu Sans",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(ctx->cr, 180.0, 220.0);
			snprintf(buf, sizeof(buf), "%d%%", brightness);
			cairo_show_text(ctx->cr, buf);
		}
	}

	// battery
	if(ctx->modules->battery) {
		struct battery_status status = battery_get(ctx->modules->battery);
		const char* sym = battery_symbol(status);

		cairo_select_font_face(ctx->cr, "FontAwesome",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(ctx->cr, 272.0, 220.0);
		cairo_show_text(ctx->cr, sym);

		cairo_select_font_face(ctx->cr, "DejaVu Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(ctx->cr, 300.0, 220.0);
		snprintf(buf, sizeof(buf), "%d%%", status.percent);
		cairo_show_text(ctx->cr, buf);

		// wattage output incorrect while charging
		if(!status.charging) {
			sym = u8"";

			cairo_select_font_face(ctx->cr, "FontAwesome",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(ctx->cr, 392.0, 220.0);
			cairo_show_text(ctx->cr, sym);

			cairo_select_font_face(ctx->cr, "DejaVu Sans",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			snprintf(buf, sizeof(buf), "%.2f W", status.wattage);
			cairo_move_to(ctx->cr, 410.0, 220.0);
			cairo_show_text(ctx->cr, buf);
		}
	}

	// notes
	const char* notes[64];
	unsigned count = notes_get(ctx->modules->notes, notes);
	float y = 300.0;
	for(unsigned i = 0u; i < count; ++i) {
		// printf("node: %s (%d)\n", buf, len);
		cairo_move_to(ctx->cr, 32.0, y);
		cairo_show_text(ctx->cr, notes[i]);
		free((void*) notes[i]);

		y += 45;
		if(y > 480) {
			break;
		}
	}

	// finish
	cairo_surface_flush(ctx->surface);
	xcb_flush(ctx->connection);
}

void process(struct display* ctx, xcb_generic_event_t* gev) {
	switch(gev->response_type & 0x7f) {
		case XCB_MAP_NOTIFY:
		case XCB_EXPOSE:
			if(ctx->dashboard) {
				draw_dashboard(ctx);
			} else if(ctx->banner != banner_none) {
				draw_banner(ctx);
			}
			break;
		case XCB_CONFIGURE_NOTIFY: {
			xcb_configure_notify_event_t* ev = (xcb_configure_notify_event_t*) gev;
			cairo_xcb_surface_set_size(ctx->surface, ev->width, ev->height);
			printf("resizing to %d %d\n", ev->width, ev->height);
			ctx->width = ev->width;
			ctx->height = ev->height;
			break;
		} case XCB_KEY_PRESS: {
			xcb_key_press_event_t* ev = (xcb_key_press_event_t*) gev;
			if((ev->detail - 8) == KEY_ESC) {
				display_unmap_dashboard(ctx);
			}
			break;
		} case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t* ev = (xcb_client_message_event_t*) gev;
			uint32_t protocol = ev->data.data32[0];
			if(protocol == ctx->atoms.wm_delete_window) {
				display_unmap_dashboard(ctx);
			} else if(protocol == ctx->ewmh._NET_WM_PING) {
				// respond with pong
				xcb_ewmh_send_wm_ping(&ctx->ewmh,
					ctx->screen->root, ev->data.data32[1]);
			}
			break;
		} case 0: {
			xcb_generic_error_t* error = (xcb_generic_error_t*) gev;
			printf("retrieved xcb error code %d\n", error->error_code);
			break;
		} default:
			break;
	}
}


static void read_xcb(int fd, unsigned revents, void* data) {
	(void) data;
	(void) fd;
	(void) revents;
	struct display* ctx = (struct display*) data;

	xcb_generic_event_t* gev;
	while((gev = xcb_poll_for_event(ctx->connection))) {
		printf("poll xcb\n");
		process(ctx, gev);
		free(gev);
	}
}

static void read_timer(int fd, unsigned revents, void* data) {
	(void) revents;
	struct display* ctx = (struct display*) data;

	uint64_t val;
	int res = read(fd, &val, sizeof(val));
	assert(res == 8);

	// hide banner
	ctx->banner = banner_none;
	xcb_unmap_window(ctx->connection, ctx->window);
	xcb_flush(ctx->connection);
}

struct display* display_create(struct modules* modules) {
	struct display* ctx = calloc(1, sizeof(*ctx));
	ctx->modules = modules;

	// setup xcb connection
	ctx->connection = xcb_connect(NULL, NULL);
	int err;
	if((err = xcb_connection_has_error(ctx->connection))) {
		printf("xcb connection error: %d\n", err);
		goto err;
	}

	// make sure connection is polled in main loop for events
	add_poll_handler(xcb_get_file_descriptor(ctx->connection), POLLIN,
		ctx, read_xcb);

	ctx->screen = xcb_setup_roots_iterator(xcb_get_setup(ctx->connection)).data;

	// load atoms
	// roundtrip only once instead of for every atom
	xcb_intern_atom_cookie_t* ewmh_cookie = xcb_ewmh_init_atoms(ctx->connection, &ctx->ewmh);

	struct {
		const char *name;
		xcb_intern_atom_cookie_t cookie;
		xcb_atom_t *atom;
	} atom[] = {
		{
			.name = "WM_DELETE_WINDOW",
			.atom = &ctx->atoms.wm_delete_window,
		},
	};

	for(size_t i = 0; i < sizeof(atom) / sizeof(atom[0]); ++i) {
		atom[i].cookie = xcb_intern_atom(ctx->connection,
			true, strlen(atom[i].name), atom[i].name);
	}

	for(size_t i = 0; i < sizeof(atom) / sizeof(atom[0]); ++i) {
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
			ctx->connection, atom[i].cookie, NULL);

		if(reply) {
			*atom[i].atom = reply->atom;
			free(reply);
		} else {
			*atom[i].atom = XCB_ATOM_NONE;
		}
	}

	xcb_ewmh_init_atoms_replies(&ctx->ewmh, ewmh_cookie, NULL);

	// init visual
	// we want a 32-bit visual
	xcb_depth_iterator_t dit = xcb_screen_allowed_depths_iterator(ctx->screen);

	xcb_visualtype_t* visualtype = NULL;
	unsigned vdepth;
	for(; dit.rem; xcb_depth_next(&dit)) {
		if(dit.data->depth != 32) {
			continue;
		}

		xcb_visualtype_iterator_t vit = xcb_depth_visuals_iterator(dit.data);
		for(; vit.rem; xcb_visualtype_next(&vit)) {
			visualtype = vit.data;
			vdepth = dit.data->depth;
			break;
		}
	}

	// if no good visual found: just use root visual
	if(!visualtype) {
		printf("Couldn't find 32-bit visual, trying root visual\n");
		for(; dit.rem; xcb_depth_next(&dit)) {
			xcb_visualtype_iterator_t vit = xcb_depth_visuals_iterator(dit.data);
			for(; vit.rem; xcb_visualtype_next(&vit)) {
				if(vit.data->visual_id == ctx->screen->root_visual) {
					visualtype = vit.data;
					vdepth = dit.data->depth;
					break;
				}
			}
		}

		assert(visualtype && "Screen root visual not found");
	}

	printf("visual: %#010x %#010x %#010x, rgb-bits %d\n",
		visualtype->red_mask,
		visualtype->green_mask,
		visualtype->blue_mask,
		visualtype->bits_per_rgb_value);

	// setup xcb window
	ctx->width = start_width;
	ctx->height = start_height;

	uint32_t mask;
  	uint32_t values[3];

	// colormap: it seems like we have to create a colormap for transparent
	// windows, otherwise they won't display correctly.
	ctx->colormap = xcb_generate_id(ctx->connection);
	XCB_CHECK(xcb_create_colormap_checked(ctx->connection, XCB_COLORMAP_ALLOC_NONE,
		ctx->colormap, ctx->screen->root, visualtype->visual_id));

	// have to specify border pixel (with colormap), otherwise we get bad match
	// not exactly sure why; x11... there may have been a valid reason
	// for this like 30 years ago
	mask = XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
	values[0] = 0;
	values[1] = XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	values[2] = ctx->colormap;

	ctx->window = xcb_generate_id(ctx->connection);
	XCB_CHECK(xcb_create_window_checked(ctx->connection, vdepth, ctx->window,
		ctx->screen->root, 0, 0, ctx->width, ctx->height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, visualtype->visual_id, mask, values));

	// window properties
	// window type is dialog: floating, over other windows
	xcb_ewmh_set_wm_window_type(&ctx->ewmh, ctx->window, 1,
		&ctx->ewmh._NET_WM_WINDOW_TYPE_DIALOG);

	// title
	const char* title = "dashboard";
	xcb_ewmh_set_wm_name(&ctx->ewmh, ctx->window, strlen(title), title);

	// supported protocols
	// allows to detect when program hangs and kill it
	xcb_atom_t protocols = ctx->ewmh.WM_PROTOCOLS;
	xcb_atom_t supportedProtocols[] = {
		ctx->atoms.wm_delete_window,
		ctx->ewmh._NET_WM_PING
	};

	xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->window,
		protocols, XCB_ATOM_ATOM, 32, 2, supportedProtocols);

	// pid for ping protocol
	pid_t pid = getpid();
	xcb_ewmh_set_wm_pid(&ctx->ewmh, ctx->window, pid);

	// setup cairo
	ctx->surface = cairo_xcb_surface_create(ctx->connection,
		ctx->window, visualtype, ctx->width, ctx->height);
	ctx->cr = cairo_create(ctx->surface);

	// init timerfd for banner timeout
	ctx->timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if(ctx->timer < 0) {
		printf("timerfd_create failed: %s (%d)\n", strerror(errno), errno);
		goto err;
	}

	add_poll_handler(ctx->timer, POLLIN, ctx, read_timer);

	return ctx;

err:
	display_destroy(ctx);
	return NULL;
}

void display_map_dashboard(struct display* ctx) {
	if(ctx->dashboard) {
		return;
	}

	// disable banner timer if active
	if(ctx->banner != banner_none) {
		ctx->banner = banner_none;
		struct itimerspec unused, disable = {0};
		timerfd_settime(ctx->timer, 0, &disable, &unused);
		xcb_unmap_window(ctx->connection, ctx->window);
	}

	xcb_icccm_wm_hints_t wmhints;
	xcb_icccm_wm_hints_set_input(&wmhints, 1);
	xcb_icccm_set_wm_hints(ctx->connection, ctx->window, &wmhints);

	// TODO: don't hardcode center of screen to 1920x1080 res
	// instead use xcb_get_geometry (once, during initialization)
	xcb_size_hints_t shints;
	xcb_icccm_size_hints_set_position(&shints, 0,
		(1920 - start_width) / 2,
		(1080 - start_height) / 2);
	xcb_icccm_size_hints_set_size(&shints, 0, start_width, start_height);
	xcb_icccm_set_wm_size_hints(ctx->connection, ctx->window,
		XCB_ATOM_WM_NORMAL_HINTS, &shints);

	xcb_map_window(ctx->connection, ctx->window);
	xcb_flush(ctx->connection);
	ctx->dashboard = true;
}

void display_unmap_dashboard(struct display* ctx) {
	if(!ctx->dashboard) {
		return;
	}

	xcb_unmap_window(ctx->connection, ctx->window);
	xcb_flush(ctx->connection);
	ctx->dashboard = false;
}

void display_show_banner(struct display* ctx, enum banner banner) {
	// don't show any banners while the dashboard is active
	if(ctx->dashboard) {
		return;
	}

	if(ctx->banner == banner_none) {
		xcb_icccm_wm_hints_t wmhints;
		xcb_icccm_wm_hints_set_input(&wmhints, 0);
		xcb_icccm_set_wm_hints(ctx->connection, ctx->window, &wmhints);

		// TODO: don't hardcode center of screen to 1920x1080 res
		// instead use xcb_get_geometry (once, during initialization)
		xcb_size_hints_t shints;
		xcb_icccm_size_hints_set_position(&shints, 0,
			1920 - banner_width - banner_margin_x,
			1080 - banner_height - banner_margin_y);
		xcb_icccm_size_hints_set_size(&shints, 0, banner_width, banner_height);
		xcb_icccm_set_wm_size_hints(ctx->connection, ctx->window,
			XCB_ATOM_WM_NORMAL_HINTS, &shints);

		xcb_map_window(ctx->connection, ctx->window);
		xcb_flush(ctx->connection);
	} else {
		send_expose_event(ctx);
	}

	ctx->banner = banner;

	// set timeout on timerfd to hide the banner
	struct itimerspec timer_unused, timer = {0};
	timer.it_value.tv_sec = banner_time;
	timerfd_settime(ctx->timer, 0, &timer, &timer_unused);
}

void display_destroy(struct display* ctx) {
	if(ctx->cr) cairo_destroy(ctx->cr);
	if(ctx->surface) cairo_surface_destroy(ctx->surface);
	if(ctx->connection) {
		if(ctx->window) {
			xcb_destroy_window(ctx->connection, ctx->window);
		}

		xcb_disconnect(ctx->connection);
	}

	free(ctx);
}

void display_redraw_dashboard(struct display* ctx) {
	if(!ctx->dashboard) {
		return;
	}

	send_expose_event(ctx);
}