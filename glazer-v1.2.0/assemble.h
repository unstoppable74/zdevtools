// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* assembling code */

void InitAssembler (void);

void AssembleHeader (void);
void AssembleAll (void);
void AssembleSection (struct Node * list, enum SectionKind section);

enum AddressingMode CalcAddressingMode (struct Node * T);

// these are only for Z-code
bool AssembleSetRelease  (int number, const char * serial);
bool AssembleSetCompiler (const char * compiler);

// this "private-ish" function is needed for complex strings
void ASM_Operand (struct Node * T);

// this "private-ish" function is needed for dumping symbols
int AddressForLabel (struct LabelDef * lab);
