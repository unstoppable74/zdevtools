// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* file management */

struct Position
{
	int  line;  // 0 if unknown
	int  file;  // 0 if unknown, otherwise an interned string
};

extern struct Position  no_position;

// this is returned after reaching end-of-file
#define FETCH_EOF  0x11111E0F

// the loader will create three lists of instructions, one for
// each section type (TEXT, DATA, BSS, STATIC).
extern struct Node * stream[4];

void F_LoadCodeFile (const char * file);

void F_AddPath (const char * dir, bool is_primary);
