// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

typedef uint8_t  zscii_t;

#define ZSCII_NEWLINE  13

#define EXTRA_FIRST  155
#define EXTRA_LAST   251
#define EXTRA_NUM    (EXTRA_LAST - EXTRA_FIRST + 1)

struct TextState
{
	// the alphabet used for encoding strings
	zscii_t  alphabet[3][26];

	// these are non-NULL when a custom alphabet been specified
	const char * custom_alphabet[3];

	// the extra characters (as Unicode code points).
	// unused characters are zero.
	uchar_t  extra[EXTRA_NUM];

	// whether extra characters been explicitly specified
	bool  custom_extra;

	// while true, we are marking which extra characters we will need
	bool  auto_alloc;

	// character mapping: Unicode --> ZSCII
	zscii_t  uni_to_z[65536];

	// alphabet mapping: ZSCII --> (row << 8) + column + 1
	int  z_to_alphabet[256];
};

static struct TextState  tx;

// the default Unicode values for ZSCII characters 155 to 251.
// sourced from the Z-Machine Standards document.
// values 224 and above are not assigned.
static uchar_t default_extra[EXTRA_NUM] =
{
	0x00E4, 0x00F6, 0x00FC, 0x00C4, 0x00D6, 0x00DC, 0x00DF, 0x00BB, 0x00AB, 0x00EB,
	0x00EF, 0x00FF, 0x00CB, 0x00CF, 0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00FD,
	0x00C1, 0x00C9, 0x00CD, 0x00D3, 0x00DA, 0x00DD, 0x00E0, 0x00E8, 0x00EC, 0x00F2,
	0x00F9, 0x00C0, 0x00C8, 0x00CC, 0x00D2, 0x00D9, 0x00E2, 0x00EA, 0x00EE, 0x00F4,
	0x00FB, 0x00C2, 0x00CA, 0x00CE, 0x00D4, 0x00DB, 0x00E5, 0x00C5, 0x00F8, 0x00D8,
	0x00E3, 0x00F1, 0x00F5, 0x00C3, 0x00D1, 0x00D5, 0x00E6, 0x00C6, 0x00E7, 0x00C7,
	0x00FE, 0x00F0, 0x00DE, 0x00D0, 0x00A3, 0x0153, 0x0152, 0x00A1, 0x00BF, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

void InitText (void)
{
	// the default alphabet...
	memcpy (tx.alphabet[0], "abcdefghijklmnopqrstuvwxyz",    26);
	memcpy (tx.alphabet[1], "ABCDEFGHIJKLMNOPQRSTUVWXYZ",    26);
	memcpy (tx.alphabet[2], " \r0123456789.,!?_#'\"/\\-:()", 26);

	memset (tx.custom_alphabet, 0, sizeof(tx.custom_alphabet));

	// the default extra characters...
	int i;
	for (i = 0 ; i < EXTRA_NUM ; i++)
		tx.extra[i] = default_extra[i];

	tx.custom_extra = false;
	tx.auto_alloc   = false;
}

bool TextSetAlphabet (const char * row_name, const char * str)
{
	int row = -1;

	if (row_name[0] == 'a' || row_name[0] == 'A')
	{
		row_name++;
		row = row_name[0] - '0';
	}

	if (row < 0 || row > 2)
	{
		PostError ("illegal alphabet row (must be A0..A2)");
		return false;
	}

	if (tx.custom_alphabet[row] != NULL)
	{
		PostError ("alphabet row A%d has already been set", row);
		return false;
	}

	// FIXME: check it has 26 characters

	// we process this string later, since we need to know what the
	// final set of extra characters will be.
	tx.custom_alphabet[row] = str;
	return true;
}

bool TextSetExtraChars (const char * extra)
{
	if (tx.custom_extra)
	{
		PostError ("already set the extra characters");
		return false;
	}

	tx.custom_extra = true;

	// FIXME: TextSetExtraChars
	(void) extra;

	return true;
}

bool TextSetAutoMode (void)
{
	if (tx.custom_extra)
	{
		PostError ("already set the extra characters");
		return false;
	}

	// we will use the unicode --> zscii mapping table to keep
	// track of what non-ASCII characters have been used.
	memset (tx.uni_to_z, 0, sizeof(tx.uni_to_z));

	tx.custom_extra = true;
	tx.auto_alloc   = true;
	return true;
}

//----------------------------------------------------------------------

int TextLookupExtra (uchar_t uni_char)
{
	int i;
	for (i = 0 ; i < EXTRA_NUM ; i++)
	{
		if (tx.extra[i] == uni_char)
			return EXTRA_FIRST + i;
	}

	// not found
	return 0;
}

void TextBuildAlphabet (void)
{
	// TODO: custom alphabets

	// first  column of A2 is unusable (reserved for a full ZSCII char).
	// second column of A2 is always new-line (hard-coded in interpreters).
	tx.alphabet[2][0] = ' ';
	tx.alphabet[2][1] = ZSCII_NEWLINE;

	// build the reserve mapping : ZSCII --> alphabet position
	memset (tx.z_to_alphabet, 0, sizeof(tx.z_to_alphabet));

	int row, col;
	for (row = 0 ; row <= 2 ; row++)
	{
		for (col = 0 ; col < 26 ; col++)
		{
			if (row == 2 && col == 0)
				continue;

			zscii_t zc = tx.alphabet[row][col];
			Assert (zc != 0);

			if (tx.z_to_alphabet[zc] != 0)
				tx.z_to_alphabet[zc] = (row << 8) + col + 1;
		}
	}
}

void TextBuildMapping_Normal (void)
{
	memset (tx.uni_to_z, 0, sizeof(tx.uni_to_z));

	// ASCII chars stay the same, though we map code 10 (line-feed)
	// to code 13 (ZSCII_NEWLINE).
	int i;
	for (i = 1 ; i <= 126 ; i++)
		tx.uni_to_z[i] = i;
	
	tx.uni_to_z[10] = ZSCII_NEWLINE;

	// handle the extra characters, but ignore duplicates
	for (i = 0 ; i < EXTRA_NUM ; i++)
	{
		uchar_t uni_char = tx.extra[i];

		if (uni_char != 0)
		{
			if (tx.uni_to_z[uni_char] == 0)
				tx.uni_to_z[uni_char] = EXTRA_FIRST + i;
		}
	}
}

void TextBuildMapping_Auto (void)
{
	// determine total number of extra characters required
	int required = 0;

	uchar_t ch;
	for (ch = 0xA0 ; ch <= 0xFFFF ; ch++)
	{
		if (tx.uni_to_z[ch] == 1)
			required += 1;
	}

	if (required == 0)
	{
		tx.custom_extra = false;
		return;
	}

	if (required > EXTRA_NUM)
		Fatal_Error ("too many non-ASCII characters (found %d, max is %d)\n", required, EXTRA_NUM);

	// keep characters which occur in the standard list of extra
	// characters at the same ZSCII code point.  in the case where
	// that was *all* the required chars, we don't need to add a
	// custom Unicode translation table to the story file.

	uint8_t extra_used[256];
	memset (extra_used, 0, sizeof(extra_used));

	for (ch = 0xA0 ; ch <= 0xFFFF ; ch++)
	{
		if (tx.uni_to_z[ch] == 1)
		{
			int std = TextLookupExtra (ch);
			if (std != 0)
			{
				tx.uni_to_z[ch] = std;
				extra_used[std] = 1;

				required -= 1;
			}
		}
	}

	if (required == 0)
	{
		tx.custom_extra = false;
		return;
	}

	// remaining chars need to allocate unused ZSCII codes
	int alloc_pos = EXTRA_FIRST;

	for (ch = 0xA0 ; ch <= 0xFFFF ; ch++)
	{
		if (tx.uni_to_z[ch] == 1)
		{
			while (extra_used[alloc_pos] != 0)
				alloc_pos++;

			tx.uni_to_z[ch] = alloc_pos;
			extra_used[alloc_pos] = 1;
		}
	}

	Assert (alloc_pos <= EXTRA_LAST);
}

//----------------------------------------------------------------------

struct Z_TextBlock
{
	// current length of 5-bit Z-characters
	int  len;

	// total number of words in the data block
	int  space;

	// packed Z-characters (3 per word)
	uint16_t * data;
};

struct Z_TextBlock * TextBlock_Create (void)
{
	struct Z_TextBlock * blk = UT_Alloc (sizeof(struct Z_TextBlock));

	blk->len   = 0;
	blk->space = 32;
	blk->data  = UT_Alloc (blk->space * 2);

	return blk;
}

void TextBlock_AddRaw (struct Z_TextBlock * blk, uint16_t code)
{
	int ofs   = blk->len / 3;
	int shift = (2 - blk->len % 3) * 5;

	// grow the data buffer when out of space
	if (ofs >= blk->space)
	{
		uint32_t   new_space = blk->space + 32;
		uint16_t * new_data  = UT_Alloc (new_space * 2);

		memcpy (new_data, blk->data, blk->space);
		UT_Free (blk->data);

		blk->space = new_space;
		blk->data  = new_data;
	}

	// store the Z-character
	blk->data[ofs] |= (code << shift);
	blk->len += 1;
}

void TextBlock_Finish (struct Z_TextBlock * blk)
{
	int pad = (3 - (blk->len % 3)) % 3;

	// never create a purely empty block
	if (blk->len == 0)
		pad = 3;

	for (; pad > 0 ; pad--)
		TextBlock_AddRaw (blk, 5);

	// mark the end by setting the highest bit of last word
	blk->data[blk->len / 3 - 1] |= 0x8000;
}

void TextBlock_AddChar (struct Z_TextBlock * blk, uchar_t uni_char)
{
	// NUL and characters outside of the BMP are not legal
	// FIXME: warning for this
	if (uni_char == 0 || uni_char == BAD_CHAR || uni_char > 0xFFFF)
		uni_char = '?';

	// when the char has no mapping, we can either replace it with
	// something, or ignore it (or produce an error, but other code
	// handles that case).

	zscii_t zc = tx.uni_to_z[uni_char];
	if (zc == 0)
		zc = '?';

	if (zc == ' ')
	{
		TextBlock_AddRaw (blk, 0);
		return;
	}

	int alpha_pos = tx.z_to_alphabet[zc];

	if (alpha_pos > 0)
	{
		int row = (alpha_pos >> 8);
		int col = (alpha_pos & 255) - 1;

		// need a shift code?
		if (row > 0)
			TextBlock_AddRaw (blk, 3 + row);

		TextBlock_AddRaw (blk, col);
		return;
	}
	else
	{
		// an arbitrary ZSCII code
		TextBlock_AddRaw (blk, 5);
		TextBlock_AddRaw (blk, 6);
		TextBlock_AddRaw (blk, zc >> 5);
		TextBlock_AddRaw (blk, zc & 31);
	}
}

int TextGetLength (struct Z_TextBlock * blk)
{
	return (blk->len / 3) * 2;
}

void TextWriteBlock (struct Z_TextBlock * blk)
{
	int i;
	int total = blk->len / 3;

	for (i = 0 ; i < total ; i++)
		OUT_Word (blk->data[i]);
}

//----------------------------------------------------------------------

void TextAnalyseString (const char * str)
{
	uchar_t ch;

	while (*str != 0)
	{
		str += tweak_utf8_decode_char (&ch, str);	

		// ignore NUL, BAD_CHAR and chars outside of the BMP
		if (ch == 0 || ch == BAD_CHAR || ch > 0xFFFF)
			continue;

		// mark char as used
		tx.uni_to_z[ch] = 1;
	}
}

struct Z_TextBlock * TextEncodeString (const char * str)
{
	struct Z_TextBlock * blk = TextBlock_Create ();

	uchar_t ch;

	while (*str != 0)
	{
		str += tweak_utf8_decode_char (&ch, str);	

		// this checks for illegal characters
		TextBlock_AddChar (blk, ch);
	}

	TextBlock_Finish (blk);
	return blk;
}

void TextAnalyseNode (struct Node * node, bool encode)
{
	const char * str = Node_Str (node);

	if (encode)
		node->data.ztx = TextEncodeString (str);
	else
		TextAnalyseString (str);
}

void TextAnalyseInstruction (struct Node * ins, bool encode)
{
	struct Node * T;

	if (ins->kind == NI_Opcode)
	{
		if (ins->data.zop->flags & OPFLAG_ZTEXT)
		{
			T = ins->children;
			Assert (T->kind == NL_String);
			TextAnalyseNode (T, encode);
		}
	}
	else if (ins->kind == ND_String)
	{
		T = ins->children;
		Assert (T->kind == NL_String);
		TextAnalyseNode (T, encode);
	}
	else if (ins->kind == ND_Data)
	{
		for (T = ins->children ; T != NULL ; T = T->next)
		{
			if (T->kind == NL_String)
				TextAnalyseNode (T, encode);
		}
	}
}

void TextAnalyseSection (struct Node * list, bool encode)
{
	struct Node * ins;

	for (ins = list->children ; ins != NULL ; ins = ins->next)
	{
		TextAnalyseInstruction (ins, encode);
	}
}

void TextAnalyseAll (bool encode)
{
	TextAnalyseSection (stream[SECTION_DATA],   encode);
	TextAnalyseSection (stream[SECTION_STATIC], encode);
	TextAnalyseSection (stream[SECTION_TEXT],   encode);
}

void TextFinalize (void)
{
	Outputf ("[processing strings]\n");

	if (tx.auto_alloc)
	{
		TextAnalyseAll (false);
		TextBuildMapping_Auto ();
	}
	else
	{
		TextBuildMapping_Normal ();
	}

	// this has to happen *after* we know all the extra characters
	// (i.e. the Unicode translation table).
	TextBuildAlphabet ();

	// now encode all of the strings
	TextAnalyseAll (true);
}
