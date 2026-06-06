// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

#define MAX_ERROR_MSG  512

struct Error
{
	struct Position  pos;
	struct Error *   next;
	char             msg[MAX_ERROR_MSG];
};

static struct Error *   error_list;
static struct Position  error_pos;
static int              error_count;

void ClearErrors (void)
{
	while (error_list != NULL)
	{
		struct Error * err = error_list;
		error_list = err->next;

		UT_Free (err);
	}

	error_count = 0;
}

bool HaveErrors (void)
{
	return (error_count > 0);
}

void ShowErrors (FILE * fp)
{
	struct Error * err;

	Outputf ("[%d error%s occurred]\n", error_count, (error_count > 1) ? "s" : "");

	for (err = error_list ; err != NULL ; err = err->next)
	{
		const char * filename;

		if (err->pos.file == 0)
			filename = "???";
		else
			filename = UT_FileBase (STR_Get (err->pos.file));

		fprintf (fp, "line %d of %s: %s\n", err->pos.line, filename, err->msg);
	}

	fflush (fp);
}

int CompareErrors (const struct Error * A, const struct Error * B)
{
	if (A->pos.file != B->pos.file)
	{
		const char * AF = STR_Get (A->pos.file);
		const char * BF = STR_Get (B->pos.file);

		return strcmp (AF, BF);
	}

	int AL = A->pos.line;
	int BL = B->pos.line;

	if (AL == 0) AL = 999999;
	if (BL == 0) BL = 999999;

	return AL - BL;
}

void PostError (const char * fmt, ...)
{
	// create the error container
	struct Error * err = UT_Alloc (sizeof(struct Error));

	err->pos = error_pos;

	va_list arg_ptr;

	va_start (arg_ptr, fmt);
	vsnprintf (err->msg, sizeof(err->msg), fmt, arg_ptr);
	va_end (arg_ptr);

	// insertion-sort the new error message.
	// this is slower than using e.g. merge-sort on the final list,
	// however the total number of errors is typically not large.

	struct Error * cur  = NULL;
	struct Error * prev = NULL;

	for (cur = error_list ; cur != NULL ; prev = cur, cur = cur->next)
		if (CompareErrors (err, cur) < 0)
			break;

	// insert the new error before the existing one, or at the tail when
	// it was greater than all the existing ones.
	err->next = cur;

	if (prev != NULL)
		prev->next = err;
	else
		error_list = err;

	error_count += 1;
}

void SetErrorPos (struct Position pos)
{
	error_pos = pos;
}
