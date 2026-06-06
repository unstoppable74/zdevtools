// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* resolving labels */

extern int section_start  [4];
extern int section_padding[4];

extern int section_global_vars;
extern int section_end_of_memory;

void ResolveAll (void);

int  CharsInString (const char * str);
