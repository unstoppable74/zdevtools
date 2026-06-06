// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

void * UT_Alloc (unsigned int size);
void   UT_Free  (const void * p);

uint8_t UT_LowerCase (uint8_t ch);
uint8_t UT_UpperCase (uint8_t ch);

int    UT_Compare  (const char * A, const char * B);
char * UT_Strdup   (const char * s);
char * UT_Strcat   (const char * s, const char * t);
bool   UT_MatchArg (const char * arg, char shortname, const char * longname);

bool   UT_CurrentDate (int * year, int * month, int * day, int * hour, int * minute);
void   UT_HumanReadable (char * buf, unsigned int size, int64_t number);

uint16_t UT_Random16 (uint32_t * state);
uint32_t UT_Random32 (uint64_t * state);

void   UT_FNV_Begin (uint64_t * hash);
void   UT_FNV_Add   (uint64_t * hash, const uint8_t * data, int len);
void   UT_FNV_Int   (uint64_t * hash, uint32_t value);

//----------------------------------------------------------------------------

char       * UT_JoinPath      (const char * path, const char * rest);
bool         UT_IsAbsolute    (const char * path);
bool         UT_HasExtension  (const char * path, const char * ext);
const char * UT_FileExtension (const char * path);
const char * UT_FileBase      (const char * path);
char       * UT_FilePath      (const char * path);

FILE * UT_Open    (const char * path, const char * mode);
bool   UT_Remove  (const char * path);
bool   UT_Rename  (const char * from, const char * to);
char * UT_Getcwd  (void);
bool   UT_Chdir   (const char * path);
bool   UT_Mkdir   (const char * path);

bool   UT_FileExists   (const char * path);
bool   UT_CanWriteDir  (const char * path);
char * UT_LoadFile     (const char * path, int * length);
char * UT_LoadZ80      (const char * path, int * length);

#ifdef __cplusplus
}
#endif

#endif  /* __UTILITY_H__ */
