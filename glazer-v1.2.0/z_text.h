// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* Z-machine text encoding */

void InitText (void);

bool TextSetAlphabet   (const char * row_name, const char * str);
bool TextSetExtraChars (const char * extra);
bool TextSetAutoMode   (void);

void TextFinalize (void);

int  TextGetLength  (struct Z_TextBlock * blk);
void TextWriteBlock (struct Z_TextBlock * blk);
