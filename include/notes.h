#pragma once

struct display;
struct mod_notes;

struct mod_notes* mod_notes_create(struct display*);
void mod_notes_destroy(struct mod_notes*);
const char** mod_notes_get(struct mod_notes*, unsigned* count);
