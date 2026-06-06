// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "utility.h"

#if defined(_WIN32) || defined(_WIN64)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #ifndef __WIN32__
  #define __WIN32__
  #endif
#endif

// use my TweakUTF8 library for UTF-16 conversions
#include "tweakutf8.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __WIN32__
#define PATH_SEPARATOR  '\\'
#define PATH_SEP_STR    "\\"
#else
#define PATH_SEPARATOR  '/'
#define PATH_SEP_STR    "/"
#endif

#ifdef __WIN32__
#include <wchar.h>
#else
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern void Fatal_Error (const char * msg, ...);


//---- Memory and Strings ----------------------------------------------------

void * UT_Alloc (unsigned int size)
{
	void * p = calloc (1, size);
	if (p == NULL)
		Fatal_Error ("out of memory\n");
	return p;
}

void UT_Free (const void * p)
{
	free ((void *)p);
}

void UT_Clear (void * p, unsigned int size)
{
	memset (p, 0, size);
}

char * UT_Strdup (const char * s)
{
	size_t slen   = strlen (s);
	char * result = UT_Alloc (slen + 1);
	memcpy (result, s, slen + 1);
	return result;
}

char * UT_Strcat (const char * s, const char * t)
{
	size_t slen = strlen (s);
	size_t tlen = strlen (t);
	char * result = UT_Alloc (slen + tlen + 1);
	memcpy (result, s, slen);
	memcpy (result + slen, t, tlen + 1);
	return result;
}

uint8_t UT_LowerCase (uint8_t ch)
{
	if ('A' <= ch && ch <= 'Z')
		return ch + 32;

	return ch;
}

uint8_t UT_UpperCase (uint8_t ch)
{
	if ('a' <= ch && ch <= 'z')
		return ch - 32;

	return ch;
}

int UT_Compare (const char * A, const char * B)
{
	for (;;)
	{
		int AC = (int)UT_LowerCase ((uint8_t) *A);
		int BC = (int)UT_LowerCase ((uint8_t) *B);

		if (AC != BC) { return AC - BC; }
		if (AC ==  0) { return 0;       }

		A++; B++;
	}
}

bool UT_CurrentDate (int * year, int * month, int * day, int * hour, int * minute)
{
#ifdef __WIN32__
	SYSTEMTIME sys_time;
	GetSystemTime (&sys_time);

	*year   = sys_time.wYear;
	*month  = sys_time.wMonth;
	*day    = sys_time.wDay;
	*hour   = sys_time.wHour;
	*minute = sys_time.wMinute;

	return true;

#else /* UNIX */

	time_t epoch_time;
	struct tm * calend_time;

	if (time (&epoch_time) == (time_t)-1)
		return false;

	calend_time = localtime (&epoch_time);
	if (calend_time == NULL)
		return false;

	*year   = calend_time->tm_year + 1900;
	*month  = calend_time->tm_mon + 1;
	*day    = calend_time->tm_mday;
	*hour   = calend_time->tm_hour;
	*minute = calend_time->tm_min;

	return true;
#endif
}

void UT_HumanReadable (char * buf, unsigned int size, int64_t number)
{
	if (number < 0)
	{
		*buf++ = '-';
		size  -= 1;
		number = - number;
	}

	// the SI units, e.g. 'M' = Megabytes (1000^2)
	static const char * letters = "BKMGTPEZY";

	int64_t fraction = 0;

	int i = 0;

	// the (i == 0) test means we show bytes as a fraction of a kilobyte
	while (number >= 1000ll || i == 0)
	{
		fraction = number % 1000ll;
		number   = number / 1000ll;

		i = i + 1;

		if (letters[i] == 0)
		{
			snprintf (buf, size, "ENORMOUS");
			return;
		}
	}

	snprintf (buf, size, "%3d.%02d %c", (int)number, (int)fraction / 10, letters[i]);
}

//---- Argument Parsing ------------------------------------------------------

bool UT_MatchArg (const char * arg, char shortname, const char * longname)
{
	// a short option can only have a single `-` dash
	if (arg[1] != 0 && arg[2] == 0)
	{
		if (arg[1] == shortname)
			return true;
	}

	// a long option may have either one or two `-` dashes
	arg++;
	if (*arg == '-')
		arg++;

	if (longname != NULL)
		if (strcmp (arg, longname) == 0)
			return true;

	return false;
}

//---- Random Numbers --------------------------------------------------------

// produce a 16-bit random number.
// the output convers the full range of an unsigned 16-bit integer,
// and has a good distribution for non-cryptographic uses.
uint16_t UT_Random16 (uint32_t * state)
{
	// this multiplier is from a paper about Linear Congruental Generators
	// by Pierre L'Ecuyer.  the addition term is an arbitrary odd number.
	(*state) = (*state) * 32310901u + 17u;

	return (uint16_t) ((*state) >> 16);
}

// produce a 32-bit random number.
// the output convers the full range of an unsigned 32-bit integer,
// and has a good distribution for non-cryptographic uses.
uint32_t UT_Random32 (uint64_t * state)
{
	// this multiplier is from a paper about Linear Congruental Generators
	// by Pierre L'Ecuyer.  the addition term is an arbitrary odd number.
	(*state) = (*state) * 3935559000370003845ull + 17ull;

	return (uint32_t) ((*state) >> 32);
}

//---- Fowler-Noll-Vo Hashing ------------------------------------------------

// the logic here is based on the FNV-1a algorithm, with the
// following changes:
// -  initial value is different
// -  each data byte is adjusted to avoid zero

void UT_FNV_Begin (uint64_t * hash)
{
	*hash = 1234;
}

void UT_FNV_Add (uint64_t * hash, const uint8_t * data, int len)
{
	uint64_t cur = *hash;

	for (; len > 0 ; len--, data++)
	{
		uint64_t value = (uint64_t) *data + (uint64_t) 1;

		cur ^= value;
		cur *= 1099511628211ull;
	}

	*hash = cur;
}

void UT_FNV_Int (uint64_t * hash, uint32_t value)
{
	uint8_t buf[4];

	buf[0] = (value      ) & 0xFF;
	buf[1] = (value >>  8) & 0xFF;
	buf[2] = (value >> 16) & 0xFF;
	buf[3] = (value >> 24) & 0xFF;

	UT_FNV_Add (hash, buf, 4);
}

//---- Pathnames -------------------------------------------------------------

const char * UT_FileExtension (const char * path)
{
	int len = (int)strlen (path);
	if (len == 0)
		return NULL;

	const char * p = path + (len - 1);

	for (; p > path ; p--)
	{
		char ch = p[-1];

		if (ch == '.')
			return p;

		if (ch == '/')
			break;

#ifdef __WIN32__
		if (ch == '\\' || ch == ':')
			break;
#endif
	}

	return NULL;
}

const char * UT_FileBase (const char * path)
{
	// example: "C:\Foo\Bar.txt" --> "Bar.txt"

	int len = (int)strlen (path);
	if (len == 0)
		return path;

	const char * p = path + (len - 1);

	for (; p > path ; p--)
	{
		char ch = p[-1];

		if (ch == '/')
			break;

#ifdef __WIN32__
		if (ch == '\\' || ch == ':')
			break;
#endif
	}

	return p;
}

char * UT_FilePath (const char * path)
{
	const char * p = UT_FileBase (path);

	if (p == path)
		return UT_Strdup (".");

	// we keep a trailing slash when at top of filesystem.
	// for example: "/foo.txt"   --> "/"
	//              "C:\bar.txt" --> "C:\"

	if (p == path+1 && path[0] == '/')
		return UT_Strdup ("/");

#ifdef __WIN32__
	if (p == path+1 && path[0] == '\\')
		return UT_Strdup ("\\");

	if (p == path+3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
	{
		char buffer[8];
		snprintf (buffer, sizeof(buffer), "%c:%c", path[0], path[2]);
		return UT_Strdup (buffer);
	}

	if (p == path+2 && path[1] == ':')
	{
		char buffer[8];
		snprintf (buffer, sizeof(buffer), "%c:.", path[0]);
		return UT_Strdup (buffer);
	}
#endif

	// copy the front portion...
	int    len = (int)(p - path);
	char * out = UT_Alloc (len + 1);

	memcpy (out, path, (unsigned int)len);
	out[len] = 0;

	return out;
}

bool UT_HasExtension (const char * path, const char * ext)
{
	path = UT_FileExtension (path);
	if (path == NULL)
		return false;

	if (ext == NULL || ext[0] == 0)
		return false;

	return (UT_Compare (path, ext) == 0);
}

bool UT_IsAbsolute (const char * path)
{
	if (path[0] == '/')
		return true;

#ifdef __WIN32__
	if (path[0] == '\\')
		return true;

	if (path[0] != 0 && path[1] == ':' && path[2] == '\\')
		return true;
#endif

	return false;
}

char * UT_JoinPath (const char * dir, const char * rest)
{
	size_t len1   = strlen (dir);
	size_t len2   = strlen (rest);

	char * result = UT_Alloc (len1 + len2 + 2);

	memcpy (result, dir, len1);

	if (! (len1 > 0 && (dir[len1 - 1] == '/' || dir[len1 - 1] == '\\')))
	{
		result[len1++] = PATH_SEPARATOR;
	}

	memcpy (result + len1, rest, len2 + 1);

	return result;
}

//---- Files and Directories -------------------------------------------------

#ifdef __WIN32__
#define WIDE_PATH_LEN  8192
static WCHAR wide_path [WIDE_PATH_LEN];
static WCHAR wide_other[WIDE_PATH_LEN];
#endif

FILE * UT_Open (const char * path, const char * mode)
{
#ifdef __WIN32__
	if (-1 == tweak_utf8_to_utf16 (wide_path, WIDE_PATH_LEN, path))
		return NULL;

	if (-1 == tweak_utf8_to_utf16 (wide_other, WIDE_PATH_LEN, mode))
		return NULL;

	return _wfopen (wide_path, wide_other);

#else /* UNIX */
	return fopen (path, mode);
#endif
}

bool UT_Remove (const char * path)
{
#ifdef __WIN32__
	if (-1 == tweak_utf8_to_utf16 (wide_path, WIDE_PATH_LEN, path))
		return false;

	return (_wremove (wide_path) == 0);

#else /* UNIX */
	return (remove (path) == 0);
#endif
}

bool UT_Rename (const char * from, const char * to)
{
#ifdef __WIN32__
	if (-1 == tweak_utf8_to_utf16 (wide_path, WIDE_PATH_LEN, from))
		return false;

	if (-1 == tweak_utf8_to_utf16 (wide_other, WIDE_PATH_LEN, to))
		return NULL;

	return (_wrename (wide_path, wide_other) == 0);

#else /* UNIX */
	return (rename (from, to) == 0);
#endif
}

char * UT_Getcwd (void)
{
#ifdef __WIN32__
	// the NULL parameter means to allocate a buffer
	WCHAR * dir = _wgetcwd (NULL, 256);
	if (dir == NULL)
		return NULL;

	size_t len = 0;
	while (dir[len] != 0)
		len++;

	// worst case for converting UTF-16 to UTF-8
	len = len * 4 + 8;
	char * buf = UT_Alloc (len);

	tweak_utf8_from_utf16 (buf, len, dir);

	free (dir);

#else /* UNIX */
	int size = 8192;

	char * buf = UT_Alloc (size);

	if (getcwd (buf, (size_t)size) == NULL)
		return NULL;
#endif

	return buf;
}

bool UT_Chdir (const char * path)
{
#ifdef __WIN32__
	if (-1 == tweak_utf8_to_utf16 (wide_path, WIDE_PATH_LEN, path))
		return false;

	return (_wchdir (wide_path) == 0);

#else /* UNIX */
	return (chdir (path) == 0);
#endif
}

bool UT_Mkdir (const char * path)
{
#ifdef __WIN32__
	if (-1 == tweak_utf8_to_utf16 (wide_path, WIDE_PATH_LEN, path))
		return false;

	return (_wmkdir (wide_path) == 0);

#else /* UNIX */
	return (mkdir (path, 0777) == 0);
#endif
}

//---- File helpers ----------------------------------------------------------

bool UT_FileExists (const char * path)
{
	FILE * fp = UT_Open (path, "rb");

	if (fp == NULL)
		return false;

	fclose (fp);
	return true;
}

bool UT_CanWriteDir (const char * path)
{
	char * test_name = UT_JoinPath (path, "abcd1234.xyz");

	FILE * fp = UT_Open (test_name, "wb");

	bool result = (fp != NULL);

	if (fp != NULL)
		fclose (fp);

	UT_Remove (test_name);
	UT_Free   (test_name);

	return result;
}

char * UT_LoadFile (const char * path, int * length)
{
	(*length) = 0;

	FILE * fp = UT_Open (path, "rb");

	if (fp == NULL)
		return NULL;

	// determine size of file (via seeking)
	fseek (fp, 0, SEEK_END);
	{
		(*length) = (int)ftell (fp);
	}
	fseek (fp, 0, SEEK_SET);

	if (ferror (fp) || (*length) < 0)
	{
		fclose (fp);
		return NULL;
	}

	char * data = UT_Alloc ((unsigned int) (1 + *length));

	// ensure the buffer is NUL-terminated
	data[*length] = 0;

	if (1 != fread (data, *length, 1, fp))
	{
		UT_Free (data);
		fclose (fp);
		return NULL;
	}

	fclose (fp);
	return data;
}

char * UT_LoadZ80 (const char * path, int * length)
{
	// the ".z80" format is common for distributing programs on certain
	// platforms, like the ZX Spectrum.  it has a header, which we ignore,
	// and is usually compressed with a simple scheme that we handle here.

	(*length) = 0;

	FILE * fp = UT_Open (path, "rb");

	if (fp == NULL)
		return NULL;

	int    space  = 4096;
	char * buffer = UT_Alloc (space);

	for (;;)
	{
		int ch = fgetc (fp);
		if (ch == EOF)
			break;

		// increase the buffer when not much space left
		if (*length > space - 300)
		{
			char * old = buffer;
			space += 4096;
			buffer = UT_Alloc (space);
			memcpy (buffer, old, (unsigned int) *length);
			UT_Free (old);
		}

		// look for a consecutive pair of 0xED bytes...
		if ((ch & 255) == 0xED)
		{
			ch = fgetc (fp);

			if (ch != EOF && (ch & 255) == 0xED)
			{
				int num  = fgetc (fp);
				int what = fgetc (fp);

				// this *normally* cannot happen...
				if (num == EOF || what == EOF)
					break;

				num = num & 255;

				for (; num > 0 ; num--)
				{
					buffer[*length] = what;
					(*length) += 1;
				}

				continue;
			}

			buffer[*length] = 0xED;
			(*length) += 1;

			if (ch == EOF)
				break;
		}

		buffer[*length] = ch;
		(*length) += 1;
	}

	// ensure the buffer is NUL-terminated
	buffer[*length] = 0;

	fclose (fp);
	return buffer;
}
