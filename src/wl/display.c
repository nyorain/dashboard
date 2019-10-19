#define _POSIX_C_SOURCE 201710L

#include <stdlib.h>
#include <wayland-client.h>
#include <cairo/cairo.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "display.h"

struct display_wl {
	struct display base;
	struct ui* ui;
	struct wl_display* display;
	struct wl_compositor* compositor;
	struct wl_shm* shm;
	struct zwlr_layer_shell_v1* layer_shell;
};

struct display* display_create_wl(struct ui* ui) {
	struct wl_display* wld = wl_display_connect(NULL);
	if(!wld) {
		return NULL;
	}

	struct display_wl* dpy = calloc(1, sizeof(*dpy));
	dpy->display = wld;
	dpy->ui = ui;

	return &dpy->base;
}
