#define _POSIX_C_SOURCE 201710L


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/inotify.h>
#include "shared.h"

static const char* nodesfile = "/home/nyorain/docs/nodes/nodes.db";
#define MAX_NOTE_COUNT 16

struct notes {
	sqlite3* db;
	sqlite3_stmt* stmt;
	int wd;

	bool valid;
	unsigned notes_count;
	const char* notes[MAX_NOTE_COUNT];
};

static void changed(const struct inotify_event* ev, void* data) {
	(void) ev;
	struct notes* notes = (struct notes*) data;
	notes->valid = false;
	display_redraw_dashboard(display_get());
}

static void free_notes(struct notes* notes) {
	for(unsigned i = 0u; i < notes->notes_count; ++i) {
		free((void*) notes->notes[i]);
	}
}

static void reload(struct notes* notes) {
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

struct notes* notes_create() {
	struct notes* notes = calloc(1, sizeof(*notes));
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
	notes_destroy(notes);
	return NULL;
}

void notes_destroy(struct notes* notes) {
	if(notes->wd) {
		rm_inotify_watch(notes->wd);
	}
	if(notes->db) {
		sqlite3_close(notes->db);
	}
	free_notes(notes);
	free(notes);
}

const char** notes_get(struct notes* notes, unsigned* count) {
	if(!notes->valid) {
		reload(notes);
	}

	*count = notes->notes_count;
	return notes->notes;
}
