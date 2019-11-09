#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/inotify.h>
#include <mainloop.h>
#include "shared.h"

struct handler {
	int wd;
	inotify_callback callback;
	void* data;
};

static struct {
	int inotify;
	struct ml_io* io;
	unsigned count;
	struct handler* handlers;
} ctx = {0};

static void poll_handler(struct ml_io* io, unsigned revents) {
	(void) revents;

	char buffer[8192];
	ssize_t nr;
	size_t n;

	while((nr = read(ctx.inotify, buffer, sizeof(buffer))) > 0) {
		for(char* p = buffer; p < buffer + nr; p += n) {
			struct inotify_event* ev = (struct inotify_event*) p;
			for(unsigned i = 0u; i < ctx.count; ++i) {
				if(ctx.handlers[i].wd == ev->wd) {
					ctx.handlers[i].callback(ev, ctx.handlers[i].data);
					break;
				}
			}

			n = sizeof(struct inotify_event) + ev->len;
		}
	}
}

int add_inotify_watch(const char* pathname, uint32_t mask,
		void* data, inotify_callback callback) {
	// initialize
	if(!ctx.inotify) {
		ctx.inotify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
		if(ctx.inotify < 0) {
			printf("Failed to init inotify: %s (%d)\n",
				strerror(errno), errno);
			return -1;
		}

		ctx.io = ml_io_new(dui_mainloop(), ctx.inotify,
			POLLIN, poll_handler);
	}

	int wd = inotify_add_watch(ctx.inotify, pathname, mask);
	if(wd < 0) {
			printf("Failed to add inotify watcher: %s (%d)\n",
				strerror(errno), errno);
		return -2;
	}

	++ctx.count;
	ctx.handlers = realloc(ctx.handlers, ctx.count * sizeof(*ctx.handlers));
	ctx.handlers[ctx.count - 1].callback = callback;
	ctx.handlers[ctx.count - 1].data = data;
	ctx.handlers[ctx.count - 1].wd = wd;

	return wd;
}

void rm_inotify_watch(int wd) {
	for(unsigned i = 0u; i < ctx.count; ++i) {
		if(ctx.handlers[i].wd == wd) {
			--ctx.count;
			inotify_rm_watch(ctx.inotify, wd);

			memmove(ctx.handlers, ctx.handlers, sizeof(*ctx.handlers) * i);
			memmove(ctx.handlers + i, ctx.handlers + i + 1,
				sizeof(*ctx.handlers) * (ctx.count - i));
			return;
		}
	}
}

