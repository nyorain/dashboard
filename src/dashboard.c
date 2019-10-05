#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>
#include <linux/input.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#include "shared.h"

struct context {
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

	bool run;

	// modules
	struct mpd* mpd;
	struct volume* volume;
	struct notes* notes;
};

// Simple macro that checks cookies returned by xcb *_checked calls
// useful when something triggers and xcb error
#define XCB_CHECK(x) {\
	xcb_void_cookie_t cookie = (x); \
	xcb_generic_error_t* gerr = xcb_request_check(ctx->connection, cookie); \
	if(gerr) { \
		printf("[%s:%d] xcb error code: %d\n", __FILE__, __LINE__, gerr->error_code); \
		free(gerr); \
	}}


bool setup(struct context* ctx) {
	// setup xcb connection
	ctx->connection = xcb_connect(NULL, NULL);
	int err;
	if((err = xcb_connection_has_error(ctx->connection))) {
		printf("xcb connection error: %d\n", err);
		return false;
	}

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
	unsigned width = 800;
	unsigned height = 500;

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
		ctx->screen->root, 0, 0, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		visualtype->visual_id, mask, values));

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

	xcb_map_window(ctx->connection, ctx->window);

	// setup cairo
	ctx->surface = cairo_xcb_surface_create(ctx->connection,
		ctx->window, visualtype, width, height);
	ctx->cr = cairo_create(ctx->surface);

	ctx->run = true;
	return true;
}

void destroy(struct context* ctx) {
	// modules
	mpd_destroy(ctx->mpd);
	volume_destroy(ctx->volume);
	notes_destroy(ctx->notes);

	// display
	cairo_destroy(ctx->cr);
	cairo_surface_destroy(ctx->surface);

	xcb_destroy_window(ctx->connection, ctx->window);
	xcb_disconnect(ctx->connection);
}

void draw(struct context* ctx) {
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
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

	const char* sym;
	if(mpd_get_playing(ctx->mpd)) {
		sym = u8"";
	} else {
		sym = u8"";
	}

	cairo_set_source_rgba(ctx->cr, 0.8, 0.8, 0.8, 0.8);
	cairo_move_to(ctx->cr, 32.0, 180.0);
	cairo_show_text(ctx->cr, sym);

	cairo_select_font_face(ctx->cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	if(!mpd_get_song(ctx->mpd, sizeof(buf), buf)) {
		buf[0] = '-';
		buf[1] = '\0';
	}

	cairo_move_to(ctx->cr, 60.0, 180.0);
	cairo_show_text(ctx->cr, buf);

	// volume
	cairo_move_to(ctx->cr, 32.0, 220.0);
	cairo_select_font_face(ctx->cr, "FontAwesome",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_show_text(ctx->cr, u8"");

	cairo_select_font_face(ctx->cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_move_to(ctx->cr, 60.0, 220.0);

	unsigned vol = volume_get(ctx->volume);
	snprintf(buf, sizeof(buf), "%d%%", vol);
	cairo_show_text(ctx->cr, buf);

	// notes
	const char* notes[64];
	unsigned count = notes_get(ctx->notes, notes);
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

void process(struct context* ctx, xcb_generic_event_t* gev) {
	switch(gev->response_type & 0x7f) {
		case XCB_MAP_NOTIFY:
		case XCB_EXPOSE:
			draw(ctx);
			break;
		case XCB_CONFIGURE_NOTIFY: {
			xcb_configure_notify_event_t* ev = (xcb_configure_notify_event_t*) gev;
			cairo_xcb_surface_set_size(ctx->surface, ev->width, ev->height);
			printf("resizing to %d %d\n", ev->width, ev->height);
			break;
		} case XCB_KEY_PRESS: {
			xcb_key_press_event_t* ev = (xcb_key_press_event_t*) gev;
			if((ev->detail - 8) == KEY_ESC) {
				// printf("escape pressed, exiting\n");
				ctx->run = false;
			}
			break;
		} case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t* ev = (xcb_client_message_event_t*) gev;
			uint32_t protocol = ev->data.data32[0];
			if(protocol == ctx->atoms.wm_delete_window) {
				// printf("received close event for window, exiting\n");
				ctx->run = false;
			} else if(protocol == ctx->ewmh._NET_WM_PING) {
				// respond with poing
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

int main() {
	struct context ctx;
	if(!setup(&ctx)) {
		return 1;
	}

	ctx.mpd = mpd_create();
	ctx.volume = volume_create();
	ctx.notes = notes_create();

	while(ctx.run) {
		xcb_generic_event_t* gev = xcb_wait_for_event(ctx.connection);
		process(&ctx, gev);
		free(gev);
	}

	destroy(&ctx);
	return 0;
}
