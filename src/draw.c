#define _POSIX_C_SOURCE 201710L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include "shared.h"

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

static const char* banner_symbol(enum banner banner, struct modules* modules) {
	switch(banner) {
		case banner_volume: return volume_get_muted(modules->volume) ? "" : "";
		case banner_brightness: return "";
		case banner_battery: return battery_symbol(battery_get(modules->battery));
		case banner_music: return mpd_state_symbol(mpd_get_state(modules->mpd));
		default: return "?";
	}
}

void draw_dashboard(cairo_surface_t* surface, cairo_t* cr, struct modules* modules) {
	// printf("drawing\n");
	char buf[256];

	// background
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	// date & time
	time_t t = time(NULL);
	struct tm tm_info;
	assert(localtime_r(&t, &tm_info));

	strftime(buf, sizeof(buf), "%H:%M", &tm_info);
	cairo_select_font_face(cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 50.0);
	cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	cairo_move_to(cr, 32.0, 70.0);
	cairo_show_text(cr, buf);

	strftime(buf, sizeof(buf), "%d.%m.%y", &tm_info);
	cairo_set_font_size(cr, 20.0);
	cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.8);
	cairo_move_to(cr, 32.0, 100.0);
	cairo_show_text(cr, buf);

	// music
	cairo_set_font_size(cr, 18.0);
	cairo_select_font_face(cr, "FontAwesome",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

	const char* sym;
	int mpdstate = mpd_get_state(modules->mpd);
	sym = mpd_state_symbol(mpdstate);

	const char* song = mpd_get_song(modules->mpd);
	if(!song || mpdstate == 1) {
		song = "-";
	}

	cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.8);
	cairo_move_to(cr, 32.0, 180.0);
	cairo_show_text(cr, sym);

	cairo_select_font_face(cr, "DejaVu Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_move_to(cr, 60.0, 180.0);
	cairo_show_text(cr, song);

	// volume
	if(modules->volume) {
		if(volume_get_muted(modules->volume)) {
			snprintf(buf, sizeof(buf), "MUTE");
			sym = u8"";
		} else {
			unsigned vol = volume_get(modules->volume);
			snprintf(buf, sizeof(buf), "%d%%", vol);
			sym = u8"";
		}

		cairo_move_to(cr, 32.0, 220.0);
		cairo_select_font_face(cr, "FontAwesome",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_show_text(cr, sym);

		cairo_select_font_face(cr, "DejaVu Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(cr, 60.0, 220.0);

		cairo_show_text(cr, buf);
	}

	// brightness
	if(modules->brightness) {
		int brightness = brightness_get(modules->brightness);
		if(brightness >= 0) {
			cairo_move_to(cr, 152.0, 220.0);
			cairo_select_font_face(cr, "FontAwesome",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_show_text(cr, u8"");

			cairo_select_font_face(cr, "DejaVu Sans",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(cr, 180.0, 220.0);
			snprintf(buf, sizeof(buf), "%d%%", brightness);
			cairo_show_text(cr, buf);
		}
	}

	// battery
	if(modules->battery) {
		struct battery_status status = battery_get(modules->battery);
		const char* sym = battery_symbol(status);

		cairo_select_font_face(cr, "FontAwesome",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(cr, 272.0, 220.0);
		cairo_show_text(cr, sym);

		cairo_select_font_face(cr, "DejaVu Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_move_to(cr, 300.0, 220.0);
		snprintf(buf, sizeof(buf), "%d%%", status.percent);
		cairo_show_text(cr, buf);

		// wattage output incorrect while charging
		if(!status.charging) {
			sym = u8"";

			cairo_select_font_face(cr, "FontAwesome",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_move_to(cr, 392.0, 220.0);
			cairo_show_text(cr, sym);

			cairo_select_font_face(cr, "DejaVu Sans",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			snprintf(buf, sizeof(buf), "%.2f W", status.wattage);
			cairo_move_to(cr, 410.0, 220.0);
			cairo_show_text(cr, buf);
		}
	}

	// notes
	const char* notes[64];
	unsigned count = notes_get(modules->notes, notes);
	float y = 300.0;
	for(unsigned i = 0u; i < count; ++i) {
		// printf("node: %s (%d)\n", buf, len);
		cairo_move_to(cr, 32.0, y);
		cairo_show_text(cr, notes[i]);
		free((void*) notes[i]);

		y += 45;
		if(y > 480) {
			break;
		}
	}

	// finish
	cairo_surface_flush(surface);
}

void draw_banner(cairo_surface_t* surface, cairo_t* cr,
		struct modules* modules, enum banner banner) {
	// background
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.8);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	const char* sym = banner_symbol(banner, modules);
	cairo_select_font_face(cr, "FontAwesome",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 30.0);
	cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	cairo_move_to(cr, 20.0, 40.0);
	cairo_show_text(cr, sym);

	if(banner == banner_volume || banner == banner_brightness) {
		unsigned percent = 0;
		if(banner == banner_volume) {
			percent = (int)(!volume_get_muted(modules->volume)) *
				volume_get(modules->volume);
		} else if(banner == banner_brightness) {
			percent = brightness_get(modules->brightness);
		}

		float fp = 0.01 * percent;
		cairo_move_to(cr, 70, banner_height / 2);
		cairo_line_to(cr, 70 + fp * (banner_width - 100), banner_height / 2);
		cairo_set_source_rgba(cr, 1, 1, 1, 1);
		cairo_stroke(cr);
	} else if(banner == banner_music) {
		const char* song = mpd_get_song(modules->mpd);
		if(!song || mpd_get_state(modules->mpd) == 1) {
			song = "-";
		}

		float x = 70;
		cairo_set_font_size(cr, 18.0);
		cairo_move_to(cr, x, 35);
		cairo_select_font_face(cr, "DejaVu Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

		cairo_text_extents_t extents;
		const char* it = song;
		unsigned count = 0;

		// Trim the song string so that it will fit into the banner.
		// If it's too long, end it with '...'. Respect utf-8 encoding
		// (mpd is guaranteed to give us utf-8 output)
		// NOTE: this kind of copy-check-extents-for-utf8 logic is probably
		// useful in more than one place.
		// hacked together, should probably be tested first.
		// is allowed to (and will!) crash when the input string isn't
		// valid utf8

		char buf[256]; // buffer to copy the resulting string into
		// pointers to the last three characters in buf
		// we keep track of them so we can replace make the last three
		// chars of the string '...' (and a null terminator)
		char* last[3] = {NULL, NULL, NULL};
		while(*it != '\0') {
			unsigned len = utf8_length(it); // how many bytes does this char have?
			if(count + len + 1 >= sizeof(buf)) {
				printf("Error: utf8-copy-check-extents buffer too short\n");
				if(last[2] != NULL) {
					*last[2]++ = '.';
					*last[2]++ = '.';
					*last[2]++ = '.';
					*last[2]++ = '\0';
				}
				break;
			}

			last[2] = last[1];
			last[1] = last[0];
			last[0] = &buf[count];
			for(unsigned i = 0u; i < len && *it != '\0'; ++i) {
				buf[count++] = *it++;
			}

			// always null-terminate the current buffer
			// in case this is the last iterations (and we stay within the
			// bounds we have), we can just use buf below.
			// But it also allows us to pass last[0] (i.e. the start
			// of the current char) to cairo since cairo needs the
			// null-terminator
			buf[count] = '\0';
			cairo_text_extents(cr, last[0], &extents);
			x += extents.x_advance;
			if(x > banner_width - 20) {
				if(last[2] != NULL) {
					*last[2]++ = '.';
					*last[2]++ = '.';
					*last[2]++ = '.';
					*last[2]++ = '\0';
				}

				break;
			}
		}

		cairo_show_text(cr, buf);
	}

	// finish
	cairo_surface_flush(surface);
}
