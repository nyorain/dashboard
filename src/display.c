#include "shared.h"

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

void display_redraw_dashboard(struct display* dpy) {
	dpy->impl->redraw_dashboard(dpy);
}
