// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

#define TWEAK_IMPL
#include "tweakutf8.h"

struct MainOptions options =
{
	NULL,      //  in_file
	NULL,      //  out_file
	false,     //  show_symbols
	false,     //  zcode
	false,     //  quiet
};

//----------------------------------------------------------------------

#define OUTPUT_BUF_SIZE  4096
static char output_buffer[OUTPUT_BUF_SIZE];

void Outputf (const char * fmt, ...)
{
	va_list arg_ptr;

	va_start (arg_ptr, fmt);
	vsnprintf (output_buffer, sizeof(output_buffer), fmt, arg_ptr);
	va_end (arg_ptr);

	if (! options.quiet)
	{
		fputs (output_buffer, stdout);
		fflush (stdout);
	}
}

// show an error message and terminate the program.
void Fatal_Error (const char * fmt, ...)
{
	va_list arg_ptr;

	va_start (arg_ptr, fmt);
	vsnprintf (output_buffer, sizeof(output_buffer), fmt, arg_ptr);
	va_end (arg_ptr);

	fflush  (stdout);
	fprintf (stderr, "\nERROR: %s", output_buffer);

	exit (2);
}

void AssertFail (const char * cond_str, const char * file, int line)
{
	snprintf (output_buffer, sizeof(output_buffer),
	          "assertion '%s' failed at %s:%d\n", cond_str, file, line);

	fflush  (stdout);
	fprintf (stderr, "\nINTERNAL ERROR: %s", output_buffer);

	exit (2);
}

//----------------------------------------------------------------------

void ShowHelp (void)
{
	Outputf ("Usage:\n");
	Outputf ("    glazer [OPTIONS...] FILE.asm [-o OUTPUT.ulx]\n");
	Outputf ("\n");

	Outputf ("Available options:\n");
#if 0
	Outputf ("    -z3 .. -z8              target the Z-machine (not Glulx)\n");
	Outputf ("\n");
#endif
	Outputf ("    -o  --output  <file>    set the output filename\n");
	Outputf ("    -p  --path    <dir>     add a path for file inclusion\n");
	Outputf ("    -s  --symbols           dump the symbol table\n");
	Outputf ("\n");
	Outputf ("    -q  --quiet             inhibit progress messages\n");
	Outputf ("    -h  --help              show this help text\n");
	Outputf ("    -v  --version           show the version\n");

	exit (0);
}

void ShowVersion (void)
{
	Outputf ("glazer " VERSION_STR "\n");

	exit (0);
}

void HandleArgs (int argc, char * argv[])
{
	// skip program itself
	argv++; argc--;

	while (argc > 0)
	{
		char *arg = *argv++; argc--;

		if (arg[0] != '-')
		{
			if (options.in_file != NULL)
				Fatal_Error ("too many input files (should be only one)\n");

			options.in_file = UT_Strdup (arg);
			continue;
		}

		// handle arguments which do not have any parameter...

		if (UT_MatchArg (arg, 'h', "help"))
		{
			ShowHelp ();
			break;
		}
		if (UT_MatchArg (arg, 'v', "version") || UT_MatchArg (arg, 'V', NULL))
		{
			ShowVersion ();
			break;
		}
		if (UT_MatchArg (arg, 's', "symbols"))
		{
			options.show_symbols = true;
			continue;
		}
		if (UT_MatchArg (arg, 'q', "quiet"))
		{
			options.quiet = true;
			continue;
		}

		// handle the Z-machine option
		if (arg[1] == 'z')
		{
			if (arg[2] < '3' || arg[2] > '8' || arg[3] != 0)
				Fatal_Error ("missing or invalid Z-machine version\n"); 

#if 1
			Fatal_Error ("Z-code support is not available yet\n");
#else
			z_version = arg[2] - '0';
			z_scale_factor = (z_version < 5) ? 2 : (z_version < 8) ? 4 : 8;

			options.zcode = true;
			continue;
#endif
		}

		// handle arguments which need a parameter...

		char * parm = NULL;

		if (argc > 0 && (*argv)[0] != '-')
		{
			parm = *argv++; argc--;
		}

		if (UT_MatchArg (arg, 'o', "output"))
		{
			if (parm == NULL || strlen (parm) == 0)
				Fatal_Error ("missing filename after %s\n", arg);

			options.out_file = UT_Strdup (parm);
			continue;
		}

		if (UT_MatchArg (arg, 'p', "path"))
		{
			if (parm == NULL || strlen (parm) == 0)
				Fatal_Error ("missing directory after %s\n", arg);

			F_AddPath (parm, false);
			continue;
		}

		Fatal_Error ("unknown command-line option: %s\n", arg);
	}
}

void ValidateArgs (void)
{
	if (options.in_file == NULL)
		Fatal_Error ("no input file to load.\n");

	// determine output filename when not given explicitly
	if (options.out_file == NULL)
	{
		char * first = UT_Strdup (options.in_file);
		char * ext   = (char *) UT_FileExtension (first);

		if (ext != NULL)
			ext[-1] = 0;

		if (options.zcode)
		{
			char buffer[16];
			snprintf (buffer, sizeof(buffer), ".z%d", z_version);
			options.out_file = UT_Strcat (first, buffer);
		}
		else
		{
			options.out_file = UT_Strcat (first, ".ulx");
		}
		UT_Free (first);
	}

	// add path of input file to include search path (even ".")
	char * dir = UT_FilePath (options.in_file);
	F_AddPath (dir, true);
	UT_Free (dir);
}

//----------------------------------------------------------------------

void Initialize (void)
{
	P_InitParser ();
	InitDefinitions ();
	InitAssembler ();
	InitText ();
}

void Compile (void)
{
	// there are three passes:
	//
	// (1) reading the file into memory.
	//
	//     creates a stream of instructions for each section.
	//     this creates constants and labels as we go, but their
	//     value is not known yet.
	//
	//     at the end, we evaluate all constants.
	//     we also need to create the string decoding table now.
	//
	// (2) go through the instruction list and compute the size
	//     of each instruction (and its operands), and hence the
	//     value of each label.
	//
	//     we also resolve names to their corresponding constant
	//     or label definition during this pass.
	//
	// (3) write everything to output file.

	F_LoadCodeFile (options.in_file);

	EvaluateConstants ();

	if (HaveErrors ())
	{
		ShowErrors (stderr);
		exit (1);
	}

	if (! options.zcode)
		HUFF_ProcessStrings ();
	else
		TextFinalize ();

	if (HaveErrors ())
	{
		ShowErrors (stderr);
		exit (1);
	}

	ResolveAll ();

	if (HaveErrors ())
	{
		ShowErrors (stderr);
		exit (1);
	}

	OUT_OpenFile (options.out_file);

	AssembleHeader  ();

	HUFF_WriteTable ();

	AssembleAll ();

	OUT_CloseFile ();

	if (HaveErrors ())
	{
		// delete the broken file we created
		Outputf ("[deleting unfinished file]\n");
		UT_Remove (options.out_file);

		ShowErrors (stderr);
		exit (1);
	}

	if (options.show_symbols)
		SYM_Dump (all_symbols);

	Outputf ("[done]\n");
}

//----------------------------------------------------------------------

int main (int argc, char * argv[])
{
	// this will fatal error on invalid arguments
	HandleArgs (argc, argv);

	ValidateArgs ();

	Initialize ();

	Compile ();

	return 0;
}
