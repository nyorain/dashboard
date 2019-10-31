#define _POSIX_C_SOURCE 200809L

#include "config.h"
#ifdef WITH_NOTES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include "shared.h"
#include "notes.h"
#include "display.h"

static const char* nodesfile = "/home/nyorain/docs/nodes/nodes.db";
#define MAX_NOTE_COUNT 16

struct mod_notes {
	struct display* dpy;
	sqlite3* db;
	sqlite3_stmt* stmt_query;
	sqlite3_stmt* stmt_delete;
	sqlite3_stmt* stmt_archive;
	int wd;

	bool valid;
	unsigned notes_count;
	struct note notes[MAX_NOTE_COUNT];
};

static void changed(const struct inotify_event* ev, void* data) {
	(void) ev;
	struct mod_notes* notes = (struct mod_notes*) data;
	notes->valid = false;
	display_redraw(notes->dpy, banner_none);
}

static void free_notes(struct mod_notes* notes) {
	for(unsigned i = 0u; i < notes->notes_count; ++i) {
		free((void*) notes->notes[i].string);
	}
}

static void reload(struct mod_notes* notes) {
	unsigned count = 0;
	struct note notes_buf[MAX_NOTE_COUNT];
	int err;
	while((err = sqlite3_step(notes->stmt_query)) == SQLITE_ROW &&
			notes->notes_count < MAX_NOTE_COUNT) {
		unsigned id = sqlite3_column_int(notes->stmt_query, 0);
		const char* content = (const char*) sqlite3_column_text(notes->stmt_query, 1);
		const char* ptr = strchr(content, '\n');
		unsigned len = (ptr ? (size_t)(ptr - content) : strlen(content)) + 1;
		len = len < 256 ? len : 256;

		char* buf = calloc(1, len);
		strncpy(buf, content, len - 1);
		buf[len - 1] = '\0';

		notes_buf[count].id = id;
		notes_buf[count].string = buf;
		++count;
	}

	if(err != SQLITE_DONE) {
		printf("Can't step: %s\n", sqlite3_errmsg(notes->db));
		for(unsigned i = 0u; i < count; ++i) {
			free((void*) notes_buf[i].string);
		}
		return;
	}

	free_notes(notes);
	sqlite3_reset(notes->stmt_query);
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
			ORDER BY id DESC", -1, &notes->stmt_query, 0);
	if(err != SQLITE_OK) {
		printf("Can't prepare sqlite stmt: %s\n", sqlite3_errmsg(notes->db));
		goto err;
	}

	err = sqlite3_prepare_v2(notes->db,
		"DELETE FROM nodes WHERE id = ?1", -1, &notes->stmt_delete, 0);
	if(err != SQLITE_OK) {
		printf("Can't prepare sqlite stmt: %s\n", sqlite3_errmsg(notes->db));
		goto err;
	}

	err = sqlite3_prepare_v2(notes->db,
		"UPDATE nodes SET archived = 1 WHERE id = ?1", -1, &notes->stmt_archive, 0);
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
	if(notes->wd) rm_inotify_watch(notes->wd);
	if(notes->stmt_query) sqlite3_finalize(notes->stmt_query);
	if(notes->stmt_delete) sqlite3_finalize(notes->stmt_delete);
	if(notes->stmt_archive) sqlite3_finalize(notes->stmt_archive);
	if(notes->db) sqlite3_close(notes->db);

	free_notes(notes);
	free(notes);
}

const struct note* mod_notes_get(struct mod_notes* notes, unsigned* count) {
	if(!notes->valid) {
		reload(notes);
	}

	*count = notes->notes_count;
	return notes->notes;
}

static void forkexec(const char* exec, char* const* args) {
	// double fork since we don't actually want have anything to do
	// with this process, we don't care about it.
	// Give it to pid 1 instead.
	// TODO: forks will still use our stdout/stderr/stdin.
	// We should redirect that (or clearly label which client gave
	// which output)
	pid_t pid = fork();
	if(pid < 0) {
		printf("fork failed: %s (%d)\n", strerror(errno), errno);
		return;
	} else if(pid == 0) {
		pid = fork();
		if(pid < 0) {
			printf("(second) fork failed: %s (%d)\n", strerror(errno), errno);
			_exit(EXIT_FAILURE);
		} else if(pid == 0) {
			execv(exec, args);
			printf("execvp failed: %s (%d)\n", strerror(errno), errno);
			_exit(EXIT_FAILURE);
		}
		_exit(EXIT_SUCCESS);
	}
}

void mod_notes_open(struct mod_notes* m, unsigned id) {
	(void) m;

	char buf[64];
	snprintf(buf, sizeof(buf), "nodes e %d", id);
	char* exec = "/usr/bin/termite";
	char* args[] = {exec, "-e", buf, NULL};
	forkexec(exec, args);
}

void mod_notes_delete(struct mod_notes* notes, unsigned id) {
	sqlite3_bind_int64(notes->stmt_delete, 1, id);
	int res = sqlite3_step(notes->stmt_delete);
	if(res != SQLITE_DONE) {
		printf("Node deletion failed: %s\n", sqlite3_errmsg(notes->db));
	}

	sqlite3_reset(notes->stmt_delete);
}

void mod_notes_archive(struct mod_notes* notes, unsigned id) {
	sqlite3_bind_int64(notes->stmt_archive, 1, id);
	int res = sqlite3_step(notes->stmt_archive);
	if(res != SQLITE_DONE) {
		printf("Node archiving failed: %s\n", sqlite3_errmsg(notes->db));
	}

	sqlite3_reset(notes->stmt_archive);
}

void mod_notes_create_note(struct mod_notes* m) {
	char* nodes_exec = "nodes c -t db";
	char* exec = "/usr/bin/termite";
	char* args[] = {exec, "-e", nodes_exec, NULL};
	forkexec(exec, args);
}

#else // WITH_NOTES

#include <stdlib.h>

struct mod_notes* mod_notes_create(struct display* dpy) { return NULL; }
void mod_notes_destroy(struct mod_notes* m) {}
void mod_notes_open(struct mod_notes* m, unsigned id) {}
void mod_notes_delete(struct mod_notes* m, unsigned id) {}
void mod_notes_archive(struct mod_notes* notes, unsigned id) {}
void mod_notes_create_note(struct mod_notes* m) {}
const struct note* mod_notes_get(struct mod_notes* m, unsigned* count) {
	*count = 0;
	return NULL;
}

#endif
