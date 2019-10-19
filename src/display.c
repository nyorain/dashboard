#include "config.h"
#include "display.h"
#include <stdlib.h>

// from the respective backend files
struct display* display_create_wl(struct ui* ui);
struct display* display_create_x11(struct ui* ui);

struct display* display_create(struct ui* ui) {
	struct display* dpy;

#ifdef WITH_WL
	if((dpy = display_create_wl(ui))) {
		return dpy;
	}
#endif

#ifdef WITH_X11
	if((dpy = display_create_x11(ui))) {
		return dpy;
	}
#endif

	return NULL;
}

void display_destroy(struct display* dpy) {
	if(!dpy) {
		return;
	}

	dpy->impl->destroy(dpy);
	free(dpy);
}

void display_show_banner(struct display* dpy, enum banner banner) {
	dpy->impl->show_banner(dpy, banner);
}

void display_toggle_dashboard(struct display* dpy) {
	dpy->impl->toggle_dashboard(dpy);
}

void display_redraw(struct display* dpy, enum banner banner) {
	dpy->impl->redraw(dpy, banner);
}
