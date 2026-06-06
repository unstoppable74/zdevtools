// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"


#define MAX_INPUT_LINE  8000

struct Lexer
{
	// the source to read from
	FetchCharFunc  * fetch_func;

	// have we reached the end-of-file?
	bool  hit_eof;

	// the real position (line number) in the input file
	struct Position  pos;

	// start position of a token being scanned
	struct Position  start;

	// pending top-level lines
	struct Node * pending;

	// normally NULL, a line of tokens not yet grouped
	struct Node * unread_line;

	// position in lex.chars[] for low-level scan functions
	uchar_t * r;

	// the current line being analysed
	uchar_t  chars[MAX_INPUT_LINE];
};

static struct Lexer  lex;


// start lexing via the given fetch function.
void LEX_Begin (FetchCharFunc * fetch_func)
{
	lex.fetch_func  = fetch_func;

	lex.pos.file    = 0;
	lex.pos.line    = 0;

	lex.hit_eof     = false;
	lex.pending     = Node_New (NG_Block, "", lex.pos);
	lex.unread_line = NULL;
}

void LEX_Finish (void)
{
	// nothing needed
}

void LEX_SetPosition (struct Position pos)
{
	lex.pos = pos;
}

//----------------------------------------------------------------------

bool  LEX_AlphaNum   (uchar_t ch);
bool  LEX_WordChar   (uchar_t ch);

struct Node * LEX_Group (void);
struct Node * LEX_NextLine (void);

// Reads and parses the current file, returning a sequence of NG_Line tokens.
// It returns NL_ERROR when there was a parsing problem, and NL_EOF when the
// end of file is reached.

struct Node * LEX_Read (void)
{
	if (! Node_Empty (lex.pending))
		return Node_Pop (lex.pending);

	struct Node * T;

	if (lex.hit_eof)
		T = Node_New (NL_EOF, "", lex.pos);
	else
		T = LEX_Group ();

	// any lexing error (especially an unclosed parenthesis) can really
	// muck up future lexing, so skip forward to some "good" place in
	// the input and continue from there.

	if (T->kind == NL_ERROR)
		LEX_SkipAhead ();

	return T;
}

void LEX_PushBack (struct Node * T)
{
	Node_AddFront (lex.pending, T);
}

#define MAX_GROUP_STACK  64

struct StackElem
{
	const  char * name;
	struct Node * group;
	struct Node * line;
};

struct Node * LEX_Group (void)
{
	Assert (Node_Empty (lex.pending));

	// the tokens we will process
	struct Node * L = LEX_NextLine ();

	// the fresh line we will construct
	struct Node * line = Node_New (NG_Line, "", L->pos);

	line->num = L->num;

	// current stack for grouping logic.
	// the `top` value is index of the top element, or zero when
	// the stack is empty.
	struct StackElem  stack[MAX_GROUP_STACK];
	int  top = 0;

	char  message[256];

	for (;;)
	{
		if (L->kind == NL_ERROR)
			return L;

		// detect stack overflow (almost impossible in normal code)
		if (top > MAX_GROUP_STACK-4)
			return Node_New (NL_ERROR, "parentheses or brackets are nested too deeply", lex.pos);

		/* handle end-of-file */

		if (L->kind == NL_EOF)
		{
			if (top > 0)
			{
				// use the starting position in this error
				snprintf (message, sizeof(message), "unclosed %s group", stack[top].name);
				return Node_New (NL_ERROR, message, stack[top].group->pos);
			}

			if (! Node_Empty (line))
				return line;

			return L;
		}

		// when input line is exhaused, grab the next one *when needed*.
		// at the outer level, can simply return a pending token.

		if (Node_Empty (L))
		{
			if (top == 0)
				return line;

			L = LEX_NextLine ();
			continue;
		}

		/* get next token */

		struct Node * T = Node_Pop (L);

		/* handle openers */

		if (MatchBracket (T, "("))
		{
			top += 1;
			stack[top].name  = "()";
			stack[top].group = Node_New (NG_Expr, "", T->pos);
			continue;
		}

		if (MatchBracket (T, "["))
		{
			top += 1;
			stack[top].name  = "[]";
			stack[top].group = Node_New (NG_List, "", T->pos);
			continue;
		}

		if (MatchBracket (T, "{"))
		{
			top += 1;
			stack[top].name  = "{}";
			stack[top].group = Node_New (NG_Block, "", T->pos);
			continue;
		}

		/* handle closers */

		bool is_closer = false;

		if (MatchBracket (T, ")")) is_closer = true;
		if (MatchBracket (T, "]")) is_closer = true;
		if (MatchBracket (T, "}")) is_closer = true;

		if (is_closer)
		{
			const char * s = Node_Str (T);

			if (top > 0 && s[0] == stack[top].name[1])
			{
				// okay, it matches the current group
			}
			else
			{
				snprintf (message, sizeof(message), "stray '%s' found", s);
				return Node_New (NL_ERROR, message, T->pos);
			}

			// pop the top of the stack
			T    = stack[top].group;
			top -= 1;

			if (Node_Empty (T))
			{
				if (T->kind == NG_Expr)
					return Node_New (NL_ERROR, "nothing inside ()", T->pos);

				if (T->kind == NG_List)
					return Node_New (NL_ERROR, "nothing inside []", T->pos);

				if (T->kind == NG_Block)
					return Node_New (NL_ERROR, "nothing inside {}", T->pos);
			}
		}

		/* store the token */

		if (top > 0)
			Node_Add (stack[top].group, T);
		else
			Node_Add (line, T);
	}
}

void LEX_SkipAhead (void)
{
	Node_Clear (lex.pending);
	lex.unread_line = NULL;

	for (;;)
	{
		struct Node * line = LEX_NextLine ();

		if (line->kind == NL_EOF)
			return;

		// we must ignore errors here (the whole reason we are skipping
		// ahead is to recover from a previous lexing problem).
		if (line->kind == NL_ERROR)
			continue;

		// ignore empty lines
		if (Node_Empty (line))
			continue;

		lex.unread_line = line;
		return;
	}
}

//----------------------------------------------------------------------

struct Node * LEX_ReadComment (void);
struct Node * LEX_ReadString  (uchar_t closer);

struct Node * Scan_Chunk   (struct Node * line, int length);
struct Node * Scan_Char    (void);
struct Node * Scan_Escape  (void);
struct Node * Scan_Unicode (void);


uchar_t LEX_NextChar (void)
{
	if (lex.hit_eof)
		return FETCH_EOF;

	for (;;)
	{
		uchar_t ch = (* lex.fetch_func) ();

		if (ch == FETCH_EOF)
		{
			lex.hit_eof = true;
			return ch;
		}

		// skip carriage return
		if (ch == '\r')
			continue;

		// convert NBSP to a regular space
		if (ch == 0x00A0)
			ch = ' ';

		// turn most control characters into a space
		if (ch != '\n' && char_is_space (ch))
			return ' ';

		return ch;
	}
}

struct Node * LEX_Unfinished (struct Node * err)
{
	// this ensures that LEX_NextLine() always reads a full line,
	// even when an error occurs somewhere in the middle of a line.

	for (;;)
	{
		uchar_t ch = LEX_NextChar ();

		if (ch == FETCH_EOF || ch == '\n')
			return err;
	}
}

// this reads characters from the file (etc), builds up a line in the
// `lex.chars[]` array, then scans it to produce a sequence of low-level
// tokens.  these are returned in an NG_Line, which may be empty.
//
// if there was a lexing problem, like a malformed number, then an
// NL_ERROR token is returned.  when the file reaches the end, any
// valid tokens are returned first, then the NL_EOF token.
//
// higher level code is responsible for grouping of parentheses
// and curly/square brackets.

struct Node * LEX_NextLine (void)
{
	if (lex.unread_line != NULL)
	{
		struct Node * line = lex.unread_line;
		lex.unread_line = NULL;
		return line;
	}

	if (lex.hit_eof)
		return Node_New (NL_EOF, "", lex.pos);

	lex.start = lex.pos;

	struct Node * line = Node_New (NG_Line, "", lex.pos);

	int   length = 0;

	uchar_t ch = LEX_NextChar ();

	for (;;)
	{
		if (length == 0)
			lex.start = lex.pos;

		if (length > MAX_INPUT_LINE-20)
			return LEX_Unfinished (Node_New (NL_ERROR, "extremely long line", lex.start));

		// handle EOF and EOLN
		if (ch == FETCH_EOF || ch == '\n')
			break;

		// handle single-line comments
		if (ch == ';')
		{
			// skip all remaining chars on the line
			for (;;)
			{
				ch = LEX_NextChar ();
				if (ch == FETCH_EOF || ch == '\n')
					break;
			}

			continue;
		}

		// handle string and char literals
		if (ch == '"' || ch == '`')
		{
			struct Node * tok = Scan_Chunk (line, length);
			if (tok->kind == NL_ERROR)
				return LEX_Unfinished (tok);

			length = 0;

			tok = LEX_ReadString (ch);
			if (tok->kind == NL_ERROR)
				return LEX_Unfinished (tok);

			Node_Add (line, tok);

			ch = LEX_NextChar ();
			continue;
		}

		lex.chars[length++] = ch;

		ch = LEX_NextChar ();
	}

	return Scan_Chunk (line, length);
}

struct Node * LEX_ReadString (uchar_t closer)
{
	int node_kind = (closer == '`') ? NL_Char : NL_String;
	int length    = 0;

	uchar_t ch = LEX_NextChar ();

	while (ch != closer)
	{
		if (length > MAX_INPUT_LINE-20)
			return Node_New (NL_ERROR, "extremely long string", lex.start);

		if (ch == FETCH_EOF || ch == '\n')
			return Node_New (NL_ERROR, "unclosed string", lex.start);

		// validate escapes
		if (ch == '\\')
		{
			ch = LEX_NextChar ();

			if (ch == FETCH_EOF || char_is_space (ch))
				return Node_New (NL_ERROR, "bad escape in string", lex.start);

			lex.chars[length++] = '\\';
		}

		lex.chars[length++] = ch;

		ch = LEX_NextChar ();
	}

	lex.chars[length] = 0;
	lex.r = lex.chars;

	if (node_kind == NL_Char)
		return Scan_Char ();

	// reprocess the string, converting to UTF-8
	static char  buffer[MAX_INPUT_LINE];
	int          b_len = 0;

	while (lex.r[0] != 0)
	{
		uchar_t ch = lex.r[0];

		// convert escapes
		if (ch == '\\')
		{
			struct Node * tok = Scan_Escape ();
			if (tok->kind == NL_ERROR)
				return tok;

			ch = (uchar_t) tok->num;
		}
		else
		{
			lex.r += 1;
		}

		if (b_len > (int)sizeof(buffer)-10)
			return Node_New (NL_ERROR, "extremely long string", lex.start);

		b_len += tweak_utf8_encode_char (&buffer[b_len], ch);
	}

	buffer[b_len] = 0;

	struct Node * tok = Node_New (node_kind, buffer, lex.start);
	return tok;
}

//----------------------------------------------------------------------

struct Node * Scan_Number  (void);
struct Node * Scan_Float   (void);
struct Node * Scan_Word    (int node_kind);
struct Node * Scan_Bracket (uchar_t ch);
struct Node * Scan_Symbol  (void);
struct Node * Scan_EntityNum (void);


struct Node * Scan_Chunk (struct Node * line, int length)
{
	if (length == 0)
		return line;

	lex.chars[length] = 0;

	// process the line, beginning at the beginning
	lex.r = lex.chars;

	if (Node_Empty (line) && lex.r[0] != 0 && char_is_space (lex.r[0]))
		line->num = 1;

	for (;;)
	{
		uchar_t ch = lex.r[0];

		// all done?
		if (ch == 0)
			return line;

		// whitespace ?
		if (char_is_space (ch))
		{
			lex.r += 1;
			continue;
		}

		struct Node * tok = NULL;

		// a number ?
		if (char_is_digit (ch))
		{
			tok = Scan_Number ();
		}

		// a signed number ?
		else if ((ch == '-' || ch == '+') && char_is_digit (lex.r[1]))
		{
			lex.r += 1;
			tok = Scan_Number ();

			if (tok->kind == NL_Integer && ch == '-')
				tok->num = 0 - tok->num;

			if (tok->kind == NL_Float && ch == '-')
				tok->data.flt = 0 - tok->data.flt;
		}

		// a plain word?
		else if (LEX_WordChar (ch))
		{
			tok = Scan_Word (NL_Word);
		}

		// a bracket?
		else if (ch == '(' || ch == ')' ||
		         ch == '[' || ch == ']' ||
		         ch == '{' || ch == '}')
		{
			tok = Scan_Bracket (ch);
		}

		// anything else should be a symbol
		else
			tok = Scan_Symbol ();

		Assert (tok != NULL);

		if (tok->kind == NL_ERROR)
			return tok;

		Node_Add (line, tok);
	}
}

struct Node * Scan_Bracket (uchar_t ch)
{
	char  buffer[16];
	int   b_len = tweak_utf8_encode_char (buffer, ch);

	buffer[b_len] = 0;

	lex.r += 1;
	return Node_New (NL_Bracket, buffer, lex.start);
}

struct Node * Scan_Symbol (void)
{
	Assert (lex.r[0] > ' ');

	static const char *const double_symbols[] =
	{
		"/=", "=<", "<=", ">=", "=>",
		"<<", ">>", "->", "<-", "**",
		NULL
	};

	// check for a triple symbol
	if (lex.r[0] == '>' && lex.r[1] == '>' && lex.r[2] == '>')
	{
		lex.r += 3;
		return Node_New (NL_Symbol, ">>>", lex.start);
	}

	// check for a double symbol
	if (lex.r[0] < 127 && lex.r[1] < 127)
	{
		int i = 0;
		for (; double_symbols[i] != NULL ; i++)
		{
			if (lex.r[0] == (uchar_t) double_symbols[i][0] &&
			    lex.r[1] == (uchar_t) double_symbols[i][1])
			{
				lex.r += 2;
				return Node_New (NL_Symbol, double_symbols[i], lex.start);
			}
		}
	}

	// convert character to UTF-8
	char  buffer[16];
	int   b_len = tweak_utf8_encode_char (buffer, lex.r[0]);

	buffer[b_len] = 0;

	struct Node * tok = Node_New (NL_Symbol, buffer, lex.start);

	lex.r += 1;
	return tok;
}

struct Node * Scan_Word (int node_kind)
{
	// first character has been vetted already
	int length = 1;

	for (;;)
	{
		uchar_t ch = lex.r[length];

		if (ch == 0 || ! LEX_WordChar (ch))
			break;

		length++;
	}

	// convert to UTF-8 encoding
	char  buffer[1024];
	int   b_len = 0;
	int   i;

	for (i = 0 ; i < length ; i++)
	{
		uchar_t ch = lex.r[i];

		if (b_len > (int)sizeof(buffer)-10)
			return Node_New (NL_ERROR, "extremely long word", lex.start);

		b_len += tweak_utf8_encode_char (&buffer[b_len], ch);
	}

	buffer[b_len] = 0;

	lex.r += length;

	struct Node * tok;

	// check for some special constants
	if (strcmp (buffer, "NAN") == 0)
		return Node_New (NL_FltSpec, buffer, lex.start);

	if (strcmp (buffer, "INFINITY") == 0)
		return Node_New (NL_FltSpec, "+INFINITY", lex.start);

	if (strcmp (buffer, "FALSE") == 0 || strcmp (buffer, "NULL") == 0)
	{
		tok = Node_New (NL_Integer, "", lex.start);
		tok->num = 0;
		return tok;
	}
	else if (strcmp (buffer, "TRUE") == 0)
	{
		tok = Node_New (NL_Integer, "", lex.start);
		tok->num = 1;
		return tok;
	}
	else if (strcmp (buffer, "STACKREF") == 0)
	{
		tok = Node_New (NL_Integer, "", lex.start);
		tok->num = -1;
		return tok;
	}

	tok = Node_New (node_kind, buffer, lex.start);
	return tok;
}

struct Node * Scan_Char (void)
{
	uchar_t ch = lex.r[0];

	if (ch == 0)
		return Node_New (NL_ERROR, "empty character literal", lex.start);

	struct Node * tok = NULL;

	if (ch == '\\')
	{
		// handle escapes
		tok = Scan_Escape ();
	}
	else
	{
		// handle normal cases like `x`
		tok = Node_New (NL_Char, "", lex.start);
		tok->num = (int)ch;

		lex.r += 1;
	}

	if (tok->kind == NL_ERROR)
		return tok;

	if (lex.r[0] != 0)
		return Node_New (NL_ERROR, "bad character literal", lex.start);

	return tok;
}

struct Node * Scan_Float (void)
{
	char  buffer[256];
	int   len = 0;

	bool  have_dot = false;
	bool  have_exp = false;

	for (;;)
	{
		uchar_t ch = lex.r[0];

		if (ch == 0)
			break;

		if (ch == '.')
		{
			if (have_dot)
				return Node_New (NL_ERROR, "bad float (misplaced '.')", lex.start);

			buffer[len++] = (char)ch;
			lex.r += 1;

			have_dot = true;
			continue;
		}

		if (ch == 'e' || ch == 'E')
		{
			if (have_exp)
				return Node_New (NL_ERROR, "bad float (misplaced 'e')", lex.start);

			buffer[len++] = 'e';
			lex.r += 1;

			ch = lex.r[0];

			// handle a sign
			if (ch == '+' || ch == '-')
			{
				buffer[len++] = ch;
				lex.r += 1;

				ch = lex.r[0];
			}

			if (ch < '0' || ch > '9')
				return Node_New (NL_ERROR, "bad float (missing exponent after 'e')", lex.start);

			have_exp = true;
			continue;
		}

		if (ch < '0' || ch > '9')
			break;

		if (len > (int)sizeof(buffer) - 8)
			return Node_New (NL_ERROR, "bad float (extremely long)", lex.start);

		// store the digit
		buffer[len++] = (char)ch;
		lex.r += 1;
	}

	buffer[len] = 0;

	Assert (len > 0);

	// convert buffer to the C `double` type
	double value = strtod (buffer, NULL);

	if (value == HUGE_VAL || value == -HUGE_VAL || ! isfinite (value))
		return Node_New (NL_ERROR, "bad float (magnitude is too large)", lex.start);

	struct Node * tok = Node_New (NL_Float, "", lex.start);
	tok->data.flt = value;
	return tok;
}

struct Node * Scan_Number (void)
{
	// check for floating point (presence of '.' or 'e')
	// WISH: support for hex floats.
	int k;
	for (k = 0 ; lex.r[k] != 0 ; k++)
	{
		if (lex.r[k] == '.' || lex.r[k] == 'e' || lex.r[k] == 'E')
			return Scan_Float ();

		if (lex.r[k] < '0' || lex.r[k] > '9')
			break;
	}

	int base   = 10;
	int length =  0;

	// compute value with 64-bit math
	int64_t value = 0;

	if (lex.r[0] == '0' && lex.r[1] == 'b')
	{
		base = 2;
		lex.r += 2;
	}
	else if (lex.r[0] == '0' && lex.r[1] == 'x')
	{
		base = 16;
		lex.r += 2;
	}

	for (;;)
	{
		uchar_t ch = lex.r[0];
		int digit  = 0;

		if (ch == 0)
			break;

		// allow separators like in `1_000_000`
		if (ch == '_' || ch == '\'')
		{
			lex.r += 1;
			continue;
		}

		if (! LEX_AlphaNum (ch))
			break;

		if (ch >= 'a')
			digit = 10 + (int)(ch - 'a');
		else if (ch >= 'A')
			digit = 10 + (int)(ch - 'A');
		else
			digit = (int)(ch - '0');

		if (digit < 0 || digit >= base)
		{
			char message[256];
			snprintf (message, sizeof(message), "bad digit '%c' for base %d number", (int)ch, base);
			return Node_New (NL_ERROR, message, lex.start);
		}

		value = value * (int64_t)base + (int64_t)digit;

		if (value > 0xFFFFFFFFll)
			return Node_New (NL_ERROR, "bad number (too large for 32 bits)", lex.start);

		lex.r  += 1;
		length += 1;
	}

	if (length == 0)
		return Node_New (NL_ERROR, "bad number (no digits)", lex.start);

	struct Node * tok = Node_New (NL_Integer, "", lex.start);
	tok->num = (int)value;
	return tok;
}

struct Node * Scan_Escape (void)
{
	// skip the backslash
	lex.r += 1;

	uchar_t ch = lex.r[0];

	if (ch == 0)
		return Node_New (NL_ERROR, "bad escape, nothing after '\\'", lex.start);

	else if (ch == 'n')
		ch = 10;  // LF

	else if (ch == 'u')
		return Scan_Unicode ();

	else if (! char_is_symbol (ch))
		return Node_New (NL_ERROR, "bad escape in char/string", lex.start);

	lex.r += 1;

	struct Node * tok = Node_New (NL_Char, "", lex.start);
	tok->num = (int)ch;
	return tok;
}

struct Node * Scan_Unicode (void)
{
	// skip the 'u'
	lex.r += 1;

	if (lex.r[0] != '{')
		return Node_New (NL_ERROR, "bad unicode escape, missing '{'", lex.start);

	lex.r += 1;

	uchar_t new_char = 0;

	for (;;)
	{
		uchar_t ch = lex.r[0];

		if (ch == 0)
			return Node_New (NL_ERROR, "bad unicode escape, missing '}'", lex.start);

		if (ch == '}')
		{
			lex.r += 1;
			break;
		}

		if (! char_is_hex_digit (ch))
			return Node_New (NL_ERROR, "bad unicode escape, not hex", lex.start);

		if (ch >= 'a')
			ch = ch - 'a' + 10;
		else if (ch >= 'A')
			ch = ch - 'A' + 10;
		else
			ch = ch - '0';

		new_char = (new_char << 4) + ch;
		lex.r   += 1;
	}

	if (new_char == 0 || new_char > 0x10FFFF)
		return Node_New (NL_ERROR, "illegal unicode char", lex.start);

	struct Node * tok = Node_New (NL_Char, "", lex.start);
	tok->num = (int)new_char;
	return tok;
}

//----------------------------------------------------------------------

bool LEX_HexDigit (uchar_t ch)
{
	if ('0' <= ch && ch <= '9') return true;
	if ('A' <= ch && ch <= 'F') return true;
	if ('a' <= ch && ch <= 'f') return true;

	return false;
}

bool LEX_AlphaNum (uchar_t ch)
{
	if (char_is_letter (ch)) return true;
	if (char_is_digit  (ch)) return true;

	return false;
}

bool LEX_WordChar (uchar_t ch)
{
	if (char_is_letter (ch)) return true;
	if (char_is_digit  (ch)) return true;

	if (ch == '_') return true;
	if (ch == '.') return true;

	return false;
}
