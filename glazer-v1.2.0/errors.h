// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* error reporting */

void  ClearErrors (void);
bool  HaveErrors (void);
void  ShowErrors (FILE * fp);
void  PostError (const char * fmt, ...);
void  SetErrorPos (struct Position pos);
