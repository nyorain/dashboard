#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <pulse/pulseaudio.h>
#include "mainloop.h"
#include "audio.h"
#include "shared.h"
#include "display.h"
#include "banner.h"

// TODO: continue!
// basically just implementing the functionality of pactl

struct mod_audio {
	struct display* dpy;
	struct pa_mainloop_api pa_api;
	struct pa_context* ctx;
	const char* default_sink;
	int sink_idx; // index of active sink

	bool ready;
	bool initialized;
	bool muted;
	unsigned volume;

	// for cycle-output
	const char* next_sink;
};

// paml_io
struct paml_io_data {
	void* data;
	pa_io_event_cb_t cb;
	pa_io_event_destroy_cb_t destroy_cb;
	struct pa_mainloop_api* api;
};

static void paml_io_cb(struct ml_io* io, unsigned revents) {
	struct paml_io_data* iod = ml_io_get_data(io);
	assert(iod->cb);

	int fd = ml_io_get_fd(io);
	pa_io_event_flags_t pa_revents =
		(revents & POLLIN ? PA_IO_EVENT_INPUT : 0) |
		(revents & POLLOUT ? PA_IO_EVENT_OUTPUT : 0) |
		(revents & POLLERR ? PA_IO_EVENT_ERROR : 0) |
		(revents & POLLHUP ? PA_IO_EVENT_HANGUP : 0);
	iod->cb(iod->api, (pa_io_event*) io, fd, pa_revents, iod->data);
}

static pa_io_event* paml_io_new(pa_mainloop_api* api, int fd,
		pa_io_event_flags_t pa_events, pa_io_event_cb_t cb, void* data) {
	struct mainloop* ml = (struct mainloop*) api->userdata;
	unsigned events =
		(pa_events & PA_IO_EVENT_INPUT ? POLLIN : 0) |
		(pa_events & PA_IO_EVENT_OUTPUT ? POLLOUT : 0);
	struct ml_io* io = ml_io_new(ml, fd, events, &paml_io_cb);

	struct paml_io_data* iod = calloc(1, sizeof(*iod));
	iod->data = data;
	iod->cb = cb;
	iod->api = api;
	ml_io_set_data(io, iod);
	return (pa_io_event*) io;
}

static void paml_io_enable(pa_io_event* e, pa_io_event_flags_t pa_events) {
	unsigned events =
		(pa_events & PA_IO_EVENT_INPUT ? POLLIN : 0) |
		(pa_events & PA_IO_EVENT_OUTPUT ? POLLOUT : 0);
	ml_io_set_events((struct ml_io*) e, events);
}

static void paml_io_free(pa_io_event* e) {
	if(!e) {
		return;
	}

	struct paml_io_data* dd = ml_io_get_data((struct ml_io*) e);
	if(dd->destroy_cb) {
		dd->destroy_cb(dd->api, e, dd->data);
	}
	free(dd);
	ml_io_destroy((struct ml_io*) e);
}

static void paml_io_set_destroy(pa_io_event* e, pa_io_event_destroy_cb_t cb) {
	struct paml_io_data* iod = ml_io_get_data((struct ml_io*) e);
	iod->destroy_cb = cb;
}

// paml_time
struct paml_time_data {
	void* data;
	pa_time_event_cb_t cb;
	pa_time_event_destroy_cb_t destroy_cb;
	struct pa_mainloop_api* api;
};

static void paml_time_cb(struct ml_timer* t) {
	struct paml_time_data* td = ml_timer_get_data(t);
	assert(td->cb);

	struct timespec time = ml_timer_get_time(t);
	struct timeval tv = {time.tv_sec, time.tv_nsec / 1000};
	td->cb(td->api, (pa_time_event*) t, &tv, td->data);
}

static pa_time_event* paml_time_new(pa_mainloop_api* api,
		const struct timeval* tv, pa_time_event_cb_t cb, void* data) {
	struct mainloop* ml = (struct mainloop*) api->userdata;
	struct timespec ts = { tv->tv_sec, tv->tv_usec * 1000 };
	struct ml_timer* t = ml_timer_new(ml, &ts, &paml_time_cb);

	struct paml_time_data* td = calloc(1, sizeof(*td));
	td->data = data;
	td->cb = cb;
	td->api = api;
	ml_timer_set_data(t, td);
	return (pa_time_event*) t;
}

static void paml_time_restart(pa_time_event* e, const struct timeval* tv) {
	if(!tv) {
		ml_timer_disable((struct ml_timer*) e);
	} else {
		struct timespec ts = {tv->tv_sec, 1000 * tv->tv_usec};
		ml_timer_set_time((struct ml_timer*) e, ts);
	}
}

static void paml_time_free(pa_time_event* e) {
	if(!e) {
		return;
	}

	struct paml_time_data* dd = ml_timer_get_data((struct ml_timer*) e);
	if(dd->destroy_cb) {
		dd->destroy_cb(dd->api, e, dd->data);
	}
	free(dd);
	ml_timer_destroy((struct ml_timer*) e);
}

static void paml_time_set_destroy(pa_time_event* e, pa_time_event_destroy_cb_t cb) {
	struct paml_time_data* td = ml_timer_get_data((struct ml_timer*) e);
	td->destroy_cb = cb;
}

// paml_defer
struct paml_defer_data {
	void* data;
	pa_defer_event_cb_t cb;
	pa_defer_event_destroy_cb_t destroy_cb;
	struct pa_mainloop_api* api;
};

static void paml_defer_cb(struct ml_defer* d) {
	struct paml_defer_data* dd = ml_defer_get_data(d);
	assert(dd->cb);
	dd->cb(dd->api, (pa_defer_event*) d, dd->data);
}

static pa_defer_event* paml_defer_new(pa_mainloop_api* api,
		pa_defer_event_cb_t cb, void* data) {
	struct mainloop* ml = (struct mainloop*) api->userdata;
	struct ml_defer* d = ml_defer_new(ml, &paml_defer_cb);

	struct paml_defer_data* dd = calloc(1, sizeof(*dd));
	dd->data = data;
	dd->cb = cb;
	dd->api = api;
	ml_defer_set_data(d, dd);
	return (pa_defer_event*) d;
}

static void paml_defer_enable(pa_defer_event* e, int enable) {
	ml_defer_enable((struct ml_defer*) e, (bool) enable);
}

static void paml_defer_free(pa_defer_event* e) {
	if(!e) {
		return;
	}

	struct paml_defer_data* dd = ml_defer_get_data((struct ml_defer*) e);
	if(dd->destroy_cb) {
		dd->destroy_cb(dd->api, e, dd->data);
	}
	free(dd);
	ml_defer_destroy((struct ml_defer*) e);
}

static void paml_defer_set_destroy(pa_defer_event* e, pa_defer_event_destroy_cb_t cb) {
	struct paml_defer_data* dd = ml_defer_get_data((struct ml_defer*) e);
	dd->destroy_cb = cb;
}

// other
static void paml_quit(pa_mainloop_api* api, int retval) {
	printf("paml_quit: not implemented\n");
}

static const struct pa_mainloop_api pulse_mainloop_api = {
	.io_new = paml_io_new,
	.io_enable = paml_io_enable,
	.io_free = paml_io_free,
	.io_set_destroy = paml_io_set_destroy,

	.time_new = paml_time_new,
	.time_restart = paml_time_restart,
	.time_free = paml_time_free,
	.time_set_destroy = paml_time_set_destroy,

	.defer_new = paml_defer_new,
	.defer_enable = paml_defer_enable,
	.defer_free = paml_defer_free,
	.defer_set_destroy = paml_defer_set_destroy,

	.quit = paml_quit,
};

// mod audio impl
static void get_sink_info_cb(pa_context* c, const pa_sink_info* i,
		int is_last, void* data) {
	if(is_last) {
		return;
	}

	assert(i);
	struct mod_audio* mod = data;

	if(mod->default_sink && strcmp(mod->default_sink, i->name) == 0) {
		mod->sink_idx = i->index;
		bool mute = i->mute || pa_cvolume_is_muted(&i->volume);
		uint64_t avg = pa_cvolume_avg(&i->volume);
		unsigned p = (unsigned)((avg * 100 + (uint64_t)PA_VOLUME_NORM / 2) / (uint64_t)PA_VOLUME_NORM);

		if(mute != mod->muted || mod->volume != p) {
			mod->muted = mute;
			mod->volume = p;

			// kinda hacky workaround needed due to the async model of pulse
			// with this we prevent the first reload we do to show a banner/redraw
			if(mod->initialized) {
				display_redraw(mod->dpy, banner_volume);
				display_show_banner(mod->dpy, banner_volume);
			}
			mod->initialized = true;
		}
	}
}

static void reload(struct mod_audio* mod) {
	pa_operation* o = pa_context_get_sink_info_list(mod->ctx, get_sink_info_cb, mod);
	assert(o);
	pa_operation_unref(o);
}

static void get_server_info_cb(pa_context *c, const pa_server_info *i, void *data) {
	struct mod_audio* mod = data;
	bool dreload = false;
	if(mod->default_sink) {
		if(strcmp(mod->default_sink, i->default_sink_name) == 0) {
			return;
		}

		free((void*) mod->default_sink);
		dreload = true;
	}

	mod->default_sink = strdup(i->default_sink_name);
	printf("default sink name: %s\n", i->default_sink_name);

	if(dreload) {
		reload(mod);
	}
}

static const char *subscription_event_type_to_string(pa_subscription_event_type_t t) {
    switch (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
		case PA_SUBSCRIPTION_EVENT_NEW: return "new";
		case PA_SUBSCRIPTION_EVENT_CHANGE: return "change";
		case PA_SUBSCRIPTION_EVENT_REMOVE: return "remove";
    }

    return "unknown";
}

static const char *subscription_event_facility_to_string(pa_subscription_event_type_t t) {
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK: return "sink";
		case PA_SUBSCRIPTION_EVENT_SOURCE: return "source";
		case PA_SUBSCRIPTION_EVENT_SINK_INPUT: return "sink-input";
		case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT: return "source-output";
		case PA_SUBSCRIPTION_EVENT_MODULE: return "module";
		case PA_SUBSCRIPTION_EVENT_CLIENT: return "client";
		case PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE: return "sample-cache";
		case PA_SUBSCRIPTION_EVENT_SERVER: return "server";
		case PA_SUBSCRIPTION_EVENT_CARD: return "card";
    }

    return "unknown";
}

static void pactx_subscribe_cb(pa_context* pactx,
		pa_subscription_event_type_t t, uint32_t idx, void* data) {
	struct mod_audio* mod = data;
	printf("pulse event: '%s' on '%s', %d\n",
		subscription_event_type_to_string(t),
        subscription_event_facility_to_string(t), idx);

	pa_subscription_event_type_t type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
	pa_subscription_event_type_t facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
	if(type == PA_SUBSCRIPTION_EVENT_CHANGE && facility == PA_SUBSCRIPTION_EVENT_SINK) {
		reload(mod);
	} else if(type == PA_SUBSCRIPTION_EVENT_CHANGE && facility == PA_SUBSCRIPTION_EVENT_SERVER) {
		pa_operation* o = pa_context_get_server_info(pactx, get_server_info_cb, mod);
		assert(o);
		pa_operation_unref(o);
	}
}

static void pactx_state_cb(pa_context* pactx, void* data) {
	struct mod_audio* mod = data;
	int s = pa_context_get_state(pactx);
	if(s != PA_CONTEXT_READY) {
		return;
	}

	mod->ready = true;
	pa_operation* o = NULL;
	pa_context_set_subscribe_callback(pactx, pactx_subscribe_cb, mod);
	o = pa_context_subscribe(pactx,
		PA_SUBSCRIPTION_MASK_SINK|
		PA_SUBSCRIPTION_MASK_SOURCE|
		PA_SUBSCRIPTION_MASK_SINK_INPUT|
		PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
		PA_SUBSCRIPTION_MASK_MODULE|
		PA_SUBSCRIPTION_MASK_CLIENT|
		PA_SUBSCRIPTION_MASK_SAMPLE_CACHE|
		PA_SUBSCRIPTION_MASK_SERVER|
		PA_SUBSCRIPTION_MASK_CARD, NULL, NULL);
	assert(o);
	pa_operation_unref(o);

	o = pa_context_get_server_info(pactx, get_server_info_cb, mod);
	assert(o);
	pa_operation_unref(o);

	reload(mod);
}

struct mod_audio* mod_audio_create(struct display* dpy) {
	struct mod_audio* mod = calloc(1, sizeof(*mod));
	mod->dpy = dpy;

	mod->pa_api = pulse_mainloop_api;
	mod->pa_api.userdata = dui_mainloop();
	mod->ctx = pa_context_new(&mod->pa_api, "dui");
    pa_context_set_state_callback(mod->ctx, pactx_state_cb, mod);

	if(pa_context_connect(mod->ctx, NULL, 0, NULL) < 0) {
        printf("pa_context_connect failed: %s", strerror(pa_context_errno(mod->ctx)));
		mod_audio_destroy(mod);
		return NULL;
    }

	return mod;
}

void io_destroy_paml_cb(struct ml_io* io) {
	if(ml_io_get_cb(io) == paml_io_cb) {
		paml_io_free((pa_io_event*) io);
	}
}

void timer_destroy_paml_cb(struct ml_timer* t) {
	if(ml_timer_get_cb(t) == paml_time_cb) {
		paml_time_free((pa_time_event*) t);
	}
}

void defer_destroy_paml_cb(struct ml_defer* d) {
	if(ml_defer_get_cb(d) == paml_defer_cb) {
		paml_defer_free((pa_defer_event*) d);
	}
}

void mod_audio_destroy(struct mod_audio* mod) {
	if(mod->ctx) pa_context_unref(mod->ctx);

	// make sure to correctly destroy all remaining event sources associated
	// with pulse audio
	struct mainloop* ml = dui_mainloop();
	mainloop_for_each_io(ml, io_destroy_paml_cb);
	mainloop_for_each_timer(ml, timer_destroy_paml_cb);
	mainloop_for_each_defer(ml, defer_destroy_paml_cb);
	free(mod);
}

unsigned mod_audio_get(struct mod_audio* mod) {
	return mod->volume;
}
bool mod_audio_get_muted(struct mod_audio* mod) {
	return mod->muted;
}

static void complete_cb(pa_context* c, int success, void* data) {
    if(!success) {
		int errno = pa_context_errno(c);
        printf("Pulse operation failure: %s (%d)", pa_strerror(errno), errno);
    }
}

static void move_sink_input_cb(pa_context* c, const pa_sink_input_info* i,
		int is_last, void* data) {
	struct mod_audio* mod = data;
	assert(mod->next_sink);

	if(is_last) {
		free((void*) mod->next_sink);
		mod->next_sink = NULL;
		return;
	}

	// TODO: only move if old sink is this inputs sink?
	// not exactly sure what expected behavior for custom clients is
	pa_operation* o = pa_context_move_sink_input_by_name(c, i->index,
		mod->next_sink, complete_cb, NULL);
	pa_operation_unref(o);
}

static void select_sink(struct mod_audio* mod, const char* name) {
	// set sink as default sink
	// new inputs will use that by default
	// this will trigger our internal callback and so set mod->default_sink
	pa_operation* o = pa_context_set_default_sink(mod->ctx, name, complete_cb, NULL);
	pa_operation_unref(o);

	// move all existing inputs to the new sink
	o = pa_context_get_sink_input_info_list(mod->ctx, move_sink_input_cb, mod);
	pa_operation_unref(o);
}

static void set_next_sink_cb(pa_context* c, const pa_sink_info* i,
		int is_last, void* data) {
	struct mod_audio* mod = data;
	if(is_last) {
		if(!mod->next_sink) {
			printf("cycle output: no other sink found\n");
		} else {
			select_sink(mod, mod->next_sink);
		}

		return;
	}

	// TODO: strictly speaking this logic isn't "cycling" (for more
	// than two sinks). But order here is probably not relevant
	// anyways, right?
	assert(i);
	if(!mod->next_sink && strcmp(mod->default_sink, i->name) != 0) {
		mod->next_sink = strdup(i->name);
	}
}

void mod_audio_cycle_output(struct mod_audio* mod) {
	if(!mod->ready) {
		printf("pulse audio module not in ready state\n");
		return;
	}

	if(mod->next_sink) {
		printf("cycle_output operation already pending\n");
		return;
	}

	pa_operation* o = pa_context_get_sink_info_list(mod->ctx, set_next_sink_cb, mod);
	assert(o);
	pa_operation_unref(o);
}

void mod_audio_add(struct mod_audio* mod, int percent) {
	// TODO
	printf("mod_audio_add: not implemented for pulse yet\n");
}
