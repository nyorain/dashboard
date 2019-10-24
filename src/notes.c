#define _POSIX_C_SOURCE 200809L

#include "config.h"
#ifdef WITH_NOTES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/inotify.h>
#include "shared.h"
#include "notes.h"
#include "display.h"

static const char* nodesfile = "/home/nyorain/docs/nodes/nodes.db";
#define MAX_NOTE_COUNT 16

struct mod_notes {
	struct display* dpy;
	sqlite3* db;
	sqlite3_stmt* stmt;
	int wd;

	bool valid;
	unsigned notes_count;
	const char* notes[MAX_NOTE_COUNT];
};

static void changed(const struct inotify_event* ev, void* data) {
	(void) ev;
	struct mod_notes* notes = (struct mod_notes*) data;
	notes->valid = false;
	display_redraw(notes->dpy, banner_none);
}

static void free_notes(struct mod_notes* notes) {
	for(unsigned i = 0u; i < notes->notes_count; ++i) {
		free((void*) notes->notes[i]);
	}
}

static void reload(struct mod_notes* notes) {
	unsigned count = 0;
	const char* notes_buf[MAX_NOTE_COUNT];
	int err;
	while((err = sqlite3_step(notes->stmt)) == SQLITE_ROW &&
			notes->notes_count < MAX_NOTE_COUNT) {
		const char* content = (const char*) sqlite3_column_text(notes->stmt, 1);
		const char* ptr = strchr(content, '\n');
		unsigned len = (ptr ? (size_t)(ptr - content) : strlen(content)) + 1;
		len = len < 256 ? len : 256;

		char* buf = calloc(1, len);
		strncpy(buf, content, len - 1);
		buf[len - 1] = '\0';

		notes_buf[count++] = buf;
	}

	if(err != SQLITE_DONE) {
		printf("Can't step: %s\n", sqlite3_errmsg(notes->db));
		for(unsigned i = 0u; i < count; ++i) {
			free((void*) notes_buf[i]);
		}
		return;
	}

	free_notes(notes);
	sqlite3_reset(notes->stmt);
	notes->notes_count = count;
	memcpy(notes->notes, notes_buf, sizeof(notes_buf));
	notes->valid = true;
}

struct mod_notes* mod_notes_create(struct display* dpy) {
	struct mod_notes* notes = calloc(1, sizeof(*notes));
	notes->dpy = dpy;
	int err = sqlite3_open(nodesfile, &notes->db);
	if(err != SQLITE_OK) {
		printf("Can't open sqlite database: %s\n", sqlite3_errmsg(notes->db));
		goto err;
	}

	err = sqlite3_prepare_v2(notes->db,
		"SELECT DISTINCT id, content FROM nodes \
			LEFT JOIN tags ON nodes.id = tags.node \
			WHERE tag = 'db' AND archived = 0 \
			ORDER BY id DESC", -1, &notes->stmt, 0);
	if(err != SQLITE_OK) {
		printf("Can't prepare sqlite stmt: %s\n", sqlite3_errmsg(notes->db));
		goto err;
	}

	// watch for changes in the database file
	// slightly hacky. sqlite3_update_hook doesn't work since it only
	// triggers the callback for this specific connection and not for
	// the database in general
	// TODO: not sure if this is a good idea tbh. When dashboard is open,
	// we read immediately afterwards, the database is still locked in
	// that case though...
	notes->wd = add_inotify_watch(nodesfile, IN_MODIFY, notes, changed);

	return notes;

err:
	mod_notes_destroy(notes);
	return NULL;
}

void mod_notes_destroy(struct mod_notes* notes) {
	if(!notes) {
		return;
	}
	if(notes->wd) {
		rm_inotify_watch(notes->wd);
	}
	if(notes->db) {
		sqlite3_close(notes->db);
	}
	free_notes(notes);
	free(notes);
}

const char** mod_notes_get(struct mod_notes* notes, unsigned* count) {
	if(!notes->valid) {
		reload(notes);
	}

	*count = notes->notes_count;
	return notes->notes;
}

#else // WITH_NOTES

#include <stdlib.h>

struct mod_notes* mod_notes_create(struct display* dpy) { return NULL; }
void mod_notes_destroy(struct mod_notes* m) {}
const char** mod_notes_get(struct mod_notes* m, unsigned* count) {
	*count = 0;
	return NULL;
}

#endif
