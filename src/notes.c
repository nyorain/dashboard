#define _POSIX_C_SOURCE 201710L


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sqlite3.h>
#include "shared.h"

static const char* nodesfile = "/home/nyorain/docs/nodes/nodes.db";

struct notes {
	sqlite3* db;
	sqlite3_stmt* stmt;
};

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

	return notes;

err:
	notes_destroy(notes);
	return NULL;
}

void notes_destroy(struct notes* notes) {
	if(notes->db) {
		sqlite3_close(notes->db);
	}
	free(notes);
}

unsigned notes_get(struct notes* notes, const char* contents[static 64]) {
	sqlite3_reset(notes->stmt);
	int err;
	unsigned ret = 0;
	while((err = sqlite3_step(notes->stmt)) == SQLITE_ROW && ret < 64) {
		const char* content = (const char*) sqlite3_column_text(notes->stmt, 1);
		const char* ptr = strchr(content, '\n');
		unsigned len = (ptr ? (size_t)(ptr - content) : strlen(content)) + 1;
		len = len < 256 ? len : 256;

		char* buf = calloc(1, len);
		strncpy(buf, content, len - 1);
		buf[len - 1] = '\0';

		contents[ret++] = buf;
	}

	if(err != SQLITE_DONE) {
		printf("Can't step: %s\n", sqlite3_errmsg(notes->db));
	}

	return ret;
}
