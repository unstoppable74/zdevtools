// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* Huffman coding */

void HUFF_ProcessStrings (void);

uint32_t HUFF_MeasureTable (uint32_t start);
uint32_t HUFF_TableAddress (void);
void     HUFF_WriteTable (void);

uint32_t HUFF_MeasureString (struct Node * node);
void     HUFF_WriteString   (struct Node * node);
