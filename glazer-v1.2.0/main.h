// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#define VERSION_STR  "1.2.0"
#define VERSION_NUM  (1*100 + 2*10 + 0)

/* libc includes */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <string.h>
#include <ctype.h>
#include <math.h>

/* TweakLine */

#include "tweakutf8.h"

/* types */

// a Unicode code point
typedef uint32_t uchar_t;

#define BAD_CHAR   ((uchar_t) 0xFFFD)

/* sections */

enum SectionKind
{
	SECTION_TEXT   = 0,
	SECTION_DATA   = 1,
	SECTION_BSS    = 2,  // Glulx only
	SECTION_STATIC = 3,  // Z-code only
};

/* local headers */

#include "utility.h"
#include "chars.h"
#include "intern.h"

#include "file.h"
#include "errors.h"
#include "node.h"
#include "lex.h"
#include "defs.h"
#include "eval.h"
#include "huff.h"

#include "z_opcode.h"
#include "z_text.h"

#include "opcodes.h"
#include "output.h"
#include "parse.h"
#include "symbol.h"
#include "resolve.h"
#include "assemble.h"

/* assertions */

#define Assert(cond)  \
	do { if (! (cond)) AssertFail (#cond, __FILE__, __LINE__); } while (false)

/* main */

struct MainOptions
{
	// the input filename (required)
	const char * in_file;

	// the output filename to compile, NULL means to guess one
	const char * out_file;

	// target the Z-machine instead of a Glulx VM
	bool  zcode;

	// display all the symbols after a successful compile
	bool  show_symbols;

	// be quiet, inhibit progress messages (etc)
	bool  quiet;
};

extern struct MainOptions  options;

void Outputf (const char * fmt, ...);
void Fatal_Error (const char * fmt, ...);
void AssertFail (const char * cond_str, const char * file, int line);
