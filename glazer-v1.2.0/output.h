// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* output files */

void OUT_OpenFile  (const char * filename);
void OUT_CloseFile (void);

void OUT_WriteHeader (int data_start, int bss_start, int end_of_memory,
                      int stack_size, int entry_point, int decode_table);

void OUT_Opcode   (int code);
void OUT_AddrMode (enum AddressingMode mode);
void OUT_OpData   (void);

void OUT_Byte (uint8_t  value);
void OUT_Word (uint16_t value);
void OUT_Long (uint32_t value);
void OUT_Quad (uint64_t value);

void OUT_Zeros (int count);

uint32_t GetRawFloat  (float  value);
uint64_t GetRawDouble (double value);

uint32_t GetFloatSpecial  (const char * name);
uint64_t GetDoubleSpecial (const char * name);
