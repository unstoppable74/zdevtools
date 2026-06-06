// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* string table */

int STR_Intern (const char * str);

const char * STR_Get  (int index);
const char * STR_Copy (const char * str);

uint32_t STR_Hash (const char * str);
