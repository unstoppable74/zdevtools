// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

struct Position  no_position = { 0, 0 };

//----------------------------------------------------------------------

#define MAX_INCLUDE_PATHS  20

const char * search_paths[MAX_INCLUDE_PATHS];
int  num_search_paths = 0;

void F_AddPath (const char * dir, bool is_primary)
{
	if (num_search_paths >= MAX_INCLUDE_PATHS)
		Fatal_Error ("too many include paths! (> %d)\n", MAX_INCLUDE_PATHS);

	char * copy = UT_Strdup (dir);
	int    pos  = num_search_paths;

	// primary path goes to head of the list, so shift everything up
	if (is_primary)
	{
		for (pos = MAX_INCLUDE_PATHS-1 ; pos > 0 ; pos--)
			search_paths[pos] = search_paths[pos - 1];
	}

	search_paths[pos] = copy;
	num_search_paths += 1;
}

char * F_FindInSearchPath (const char * include_file)
{
	int i;
	for (i = 0 ; i < num_search_paths ; i++)
	{
		char * filename = UT_JoinPath (search_paths[i], include_file);

		if (UT_FileExists (filename))
			return filename;

		UT_Free (filename);
	}

	// not found
	return NULL;
}

//----------------------------------------------------------------------

// we handle the `include` directive here is a rather devious
// (though some might say hacky) way, by using a stack of open
// file handles.  each `include` pushes a new handle on the top,
// and feeds characters from the new file to the lexer.  when a
// file reaches EOF, we close it and pop it off the stack.

#define MAX_INPUT_STACK  10

struct InputFile
{
	FILE * fp;
	int    short_name;

	struct Position  pos;
};

static struct InputFile  input_files[MAX_INPUT_STACK];
static int               input_total;


void F_PushOpen (const char * filename)
{
	Outputf ("[reading %s]\n", filename);

	if (input_total >= MAX_INPUT_STACK)
		Fatal_Error ("include heirarchy is too deep (or cyclic)\n");

	FILE * fp = UT_Open (filename, "rb");

	if (fp == NULL)
		Fatal_Error ("could not open file: %s\n", filename);

	struct InputFile * info = &input_files[input_total];
	input_total += 1;

	info->fp         = fp;
	info->pos.file   = STR_Intern (UT_FileBase (filename));
	info->pos.line   = 1;

	LEX_SetPosition (info->pos);
}

void F_PopClose (void)
{
	Assert (input_total > 0);

	input_total -= 1;
	struct InputFile * info = &input_files[input_total];

	fclose (info->fp);

	memset (info, 0, sizeof(*info));

	// re-establish previous position
	if (input_total > 0)
	{
		info = &input_files[input_total - 1];
		LEX_SetPosition (info->pos);
	}
}

uchar_t Fetch_FromFile (void)
{
	if (input_total == 0)
		return FETCH_EOF;

	struct InputFile * info = &input_files[input_total - 1];

	uchar_t ch = tweak_utf8_fgetc (info->fp);

	if (ch == TW_EOF)
	{
		F_PopClose ();
		return Fetch_FromFile ();
	}

	if (ch == '\n')
	{
		info->pos.line += 1;
		LEX_SetPosition (info->pos);
	}

	if (ch == TW_NUL)
		return ' ';

	return ch;
}

//----------------------------------------------------------------------

struct Node * stream[4];

static enum SectionKind  cur_section;

void F_LoadCodeDefs (void);


void F_LoadCodeFile (const char * file)
{
	// this does the first pass over an input file.
	//
	// it handles some directives, especially `include`, and can
	// create symbols for labels or constants, and a basic parsing
	// of instruction lines.
	//
	// result is three streams of instructions (one for each section),
	// each stream consisting of nothing but `NI_XXX` and `ND_XXX` nodes.
	// other processing, especially resolving expressions and addresses,
	// is done later.

	stream[SECTION_TEXT]   = Node_New (NG_List, "", no_position);
	stream[SECTION_DATA]   = Node_New (NG_List, "", no_position);
	stream[SECTION_BSS]    = Node_New (NG_List, "", no_position);
	stream[SECTION_STATIC] = Node_New (NG_List, "", no_position);

	cur_section = SECTION_TEXT;

	LEX_Begin (Fetch_FromFile);

	F_PushOpen (file);
	F_LoadCodeDefs ();

	// we should have hit EOF on the first file (causing to be closed
	// automatically).
	Assert (input_total == 0);

	LEX_Finish ();
}

void F_LoadCodeDefs ()
{
	for (;;)
	{
		struct Node * T = LEX_Read ();

		if (T->kind == NL_EOF)
			break;

		SetErrorPos (T->pos);

		if (T->kind == NL_ERROR)
		{
			PostError ("%s", Node_Str (T));
			continue;
		}

		if (Node_Empty (T))
			continue;

/* DEBUG
		Outputf ("%04d:", T->pos.line);
		Node_Dump (NULL, T);
*/
		Assert (T->kind == NG_Line);

		try_label:
		{
			struct Node * lab = P_TryParseLabel (T);
			if (lab == P_FAIL)
				continue;

			if (lab != NULL)
			{
				Node_Add (stream[cur_section], lab);
				goto try_label;
			}
		}

		if (Node_Empty (T))
			continue;

		struct Node * ins = P_ParseLine (T);
		if (ins == P_FAIL)
			continue;

		// did the parser handle the directive?
		if (ins->kind == NN_Okay)
			continue;

		// handle the `include` directive
		if (ins->kind == NI_Include)
		{
			const char * filename = Node_Str (ins);
			char * found_path = NULL;

			if (! UT_IsAbsolute (filename))
			{
				found_path = F_FindInSearchPath (filename);

				if (found_path == NULL)
					Fatal_Error ("unable to find '%s' in the search path\n", filename);

				filename = found_path;
			}

			F_PushOpen (filename);

			if (found_path != NULL)
				UT_Free (found_path);

			continue;
		}

		// handle the `section` directive
		if (ins->kind == NI_Section)
		{
			cur_section = (enum SectionKind) ins->num;
			continue;
		}

		Node_Add (stream[cur_section], ins);
	}
}
