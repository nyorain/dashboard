#pragma once

struct display;
struct mod_notes;

struct note {
	unsigned id;
	const char* string;
};

struct mod_notes* mod_notes_create(struct display*);
void mod_notes_destroy(struct mod_notes*);
const struct note* mod_notes_get(struct mod_notes*, unsigned* count);
void mod_notes_open(struct mod_notes*, unsigned id);
void mod_notes_delete(struct mod_notes*, unsigned id);
void mod_notes_archive(struct mod_notes*, unsigned id);
void mod_notes_create_note(struct mod_notes*);
