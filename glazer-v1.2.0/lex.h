// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* lexing */

// this returns a unicode character, or zero at EOF.
typedef uchar_t FetchCharFunc (void);

void  LEX_Begin (FetchCharFunc * fetch_func);
void  LEX_Finish (void);
void  LEX_SetPosition (struct Position pos);

void  LEX_SkipAhead (void);
void  LEX_PushBack (struct Node * T);

struct Node * LEX_Read (void);
