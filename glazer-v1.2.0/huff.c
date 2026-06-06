// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

#define QUICK_CHARS  0x2000

#define HFCHAR_TERMINATE  0x66000000
#define HFCHAR_INDIRECT   0x66000001
#define HFCHAR_BRANCH     0x66000002

struct HuffmanNode
{
	// this can be a `HFCHAR_XXX` special value
	uchar_t   ch;

	// weight of this char / indirect / branch
	uint32_t  weight;

	// for `HFCHAR_INDIRECT`, the NS_Indirect node
	struct Node * indirect;

	// next in the slow list
	struct HuffmanNode * next;

	// next in the sorted/merged list
	struct HuffmanNode * other;

	// links in the Huffman tree
	struct HuffmanNode * up;
	struct HuffmanNode * left;
	struct HuffmanNode * right;
};

struct HuffBlock;

struct HuffmanCodingState
{
	int  total_strings;

	struct HuffmanNode * quick[QUICK_CHARS];
	struct HuffmanNode * slow;
	struct HuffmanNode * terminator;

	struct HuffmanNode * sorted;
	struct HuffmanNode * merged;
	struct HuffmanNode * m_tail;

	struct HuffBlock   * block;

	uint32_t  table_start;
	uint32_t  table_pos;
	uint32_t  table_length;
	uint32_t  table_nodes;
	uint32_t  table_root;
};

static struct HuffmanCodingState  huff;

void HUFF_Init (void)
{
	memset (&huff, 0, sizeof(huff));
}

void HUFF_DumpASCII (void)
{
	uchar_t ch;

	for (ch = 32 ; ch <= 126 ; ch++)
	{
		if (huff.quick[ch] != NULL)
			Outputf ("freq '%c' : %d\n", ch, huff.quick[ch]->weight);
	}
}

struct HuffmanNode * HUFF_Allocate (uchar_t ch)
{
	struct HuffmanNode * N = UT_Alloc (sizeof(struct HuffmanNode));

	N->weight = 1;
	N->ch     = ch;

	if (ch == HFCHAR_BRANCH || ch == HFCHAR_TERMINATE)
	{
		// caller will handle it
	}
	else if (ch == HFCHAR_INDIRECT || ch >= QUICK_CHARS)
	{
		// add to head of the slow list
		N->next   = huff.slow;
		huff.slow = N;
	}
	else
	{
		// store in the quick list
		huff.quick[ch] = N;
	}

	return N;
}

struct HuffmanNode * HUFF_FindChar (uchar_t ch)
{
	struct HuffmanNode * N;

	for (N = huff.slow ; N != NULL ; N = N->next)
		if (N->ch == ch)
			return N;

	return NULL;
}

//----------------------------------------------------------------------

void HUFF_AddChar (uchar_t ch)
{
	struct HuffmanNode * N;

	if (ch < QUICK_CHARS)
		N = huff.quick[ch];
	else
		N = HUFF_FindChar (ch);

	if (N == NULL)
		HUFF_Allocate (ch);
	else
		N->weight += 1;
}

void HUFF_AddIndirect (struct Node * indirect)
{
	struct HuffmanNode * N = HUFF_Allocate (HFCHAR_INDIRECT);
	N->indirect = indirect;
	indirect->data.huff = N;
}

void HUFF_AnalyseStr (const char * str)
{
	uchar_t ch;

	while (*str != 0)
	{
		str += tweak_utf8_decode_char (&ch, str);	
		HUFF_AddChar (ch);
	}
}

void HUFF_Analyse (struct Node * node)
{
	huff.total_strings += 1;

	if (node->kind == NL_String)
	{
		HUFF_AnalyseStr (Node_Str (node));
		return;
	}

	Assert (node->kind == NS_Complex);

	struct Node * T;
	for (T = node->children ; T != NULL ; T = T->next)
	{
		switch (T->kind)
		{
			case NL_String:
				HUFF_AnalyseStr (Node_Str (T));
				break;

			case NL_Char:
				HUFF_AddChar ((uchar_t) T->num);
				break;

			case NS_Indirect:
				HUFF_AddIndirect (T);
				break;

			case NX_Address:
				PostError ("cannot use label expressions in complex strings");
				break;

			default:
				Fatal_Error ("BUG: unknown node in complex string\n");
				break;
		}
	}
}

//----------------------------------------------------------------------

struct HuffBlock
{
	// current length in bits
	uint32_t  len;

	// number of bytes in the data block
	uint32_t  space;

	uint8_t * data;
};

struct HuffBlock * HUFF_CreateBlock (void)
{
	struct HuffBlock * blk = UT_Alloc (sizeof(struct HuffBlock));

	blk->len   = 0;
	blk->space = 128;
	blk->data  = UT_Alloc (blk->space);

	return blk;
}

void HUFF_AddBit (struct HuffBlock * blk, int value)
{
	uint32_t ofs = blk->len >> 3;

	// grow the data buffer when out of space
	if (ofs >= blk->space)
	{
		uint32_t  new_space = blk->space + 128;
		uint8_t * new_data  = UT_Alloc (new_space);

		memcpy (new_data, blk->data, blk->space);
		UT_Free (blk->data);

		blk->space = new_space;
		blk->data  = new_data;
	}

	// store the bit
	if (value > 0)
	{
		blk->data[ofs] |= (1u << (blk->len & 7));
	}

	blk->len += 1;
}

//----------------------------------------------------------------------

struct HuffmanNode * HUFF_Sort (struct HuffmanNode * input)
{
	// NOTE: this is a merge sort

	Assert (input != NULL);

	// only a single item?  done!
	if (input->other == NULL)
		return input;

	// split into left and right lists
	struct HuffmanNode * left  = NULL;
	struct HuffmanNode * right = NULL;
	struct HuffmanNode * temp;

	while (input != NULL)
	{
		// move node from input list --> left list
		temp  = input;
		input = input->other;

		temp->other = left;
		left = temp;

		// swap lists
		temp = left;  left = right;  right = temp;
	}

	// recursively sort each side
	left  = HUFF_Sort (left);
	right = HUFF_Sort (right);

	// now merge them....

	struct HuffmanNode * tail = NULL;

	while (left != NULL || right != NULL)
	{
		// pick the lower weight
		if (left == NULL || (right != NULL && left->weight > right->weight))
		{
			temp = left;  left = right;  right = temp;
		}

		// remove the node
		temp = left;
		left = left->other;

		// add it to tail of the output list
		temp->other = NULL;

		if (tail == NULL)
			input = temp;
		else
			tail->other = temp;

		tail = temp;
	}

	return input;
}

struct HuffmanNode * HUFF_PopLowest (void)
{
	int w1 = -1;
	int w2 = -1;

	if (huff.sorted != NULL) w1 = huff.sorted->weight;
	if (huff.merged != NULL) w2 = huff.merged->weight;

	Assert (! (w1 < 0 && w2 < 0));

	struct HuffmanNode * N;

	if (w1 < 0 || (w2 >= 0 && w2 < w1))
	{
		N = huff.merged;
		huff.merged = N->other;

		if (huff.merged == NULL)
			huff.m_tail = NULL;
	}
	else
	{
		N = huff.sorted;
		huff.sorted = N->other;
	}

	return N;
}

void HUFF_BuildTree (void)
{
	if (huff.total_strings == 0)
		return;

	// create the termination node
	huff.terminator = HUFF_Allocate (HFCHAR_TERMINATE);
	huff.terminator->weight = huff.total_strings;

	// merge quick + slow nodes into one big list
	struct HuffmanNode * new_list = huff.terminator;
	struct HuffmanNode * N;

	int i;
	for (i = 0 ; i < QUICK_CHARS ; i++)
	{
		N = huff.quick[i];

		if (N != NULL)
		{
			N->other = new_list;
			new_list = N;
		}
	}

	for (N = huff.slow ; N != NULL ; N = N->next)
	{
		N->other = new_list;
		new_list = N;
	}

	// sort the list by weight, smallest first
	huff.sorted = HUFF_Sort (new_list);

	for (;;)
	{
		// is there more than one node remaining?
		int num_sorted = (huff.sorted == NULL) ? 0 :
		                 (huff.sorted->other == NULL) ? 1 : 2;

		int num_merged = (huff.merged == NULL) ? 0 :
		                 (huff.merged->other == NULL) ? 1 : 2;

		Assert (num_sorted + num_merged > 0);

		if (num_sorted + num_merged < 2)
		{
			if (num_sorted > 0)
			{
				huff.merged = huff.m_tail = huff.sorted;
				huff.sorted = NULL;
			}
			break;
		}

		// merge the two lowest nodes
		N = HUFF_Allocate (HFCHAR_BRANCH);

		N->left   = HUFF_PopLowest ();
		N->right  = HUFF_PopLowest ();
		N->weight = N->left->weight + N->right->weight;

		N->left->up  = N;
		N->right->up = N;

		if (huff.merged == NULL)
		{
			huff.merged = huff.m_tail = N;
		}
		else
		{
			// I'm 99% sure it is not possible for a merged node to
			// have a weight which is less than the current tail.
			Assert (N->weight >= huff.m_tail->weight);

			huff.m_tail->other = N;
			huff.m_tail        = N;
		}
	}
}

void HUFF_DumpTree (struct HuffmanNode * N, int depth)
{
	char buffer[64];

	if (N->ch == HFCHAR_TERMINATE)
		strcpy (buffer, "TERMINATE");
	else if (N->ch == HFCHAR_INDIRECT)
		strcpy (buffer, "INDIRECT");
	else if (N->ch == HFCHAR_BRANCH)
		strcpy (buffer, "BRANCH");
	else if (N->ch >= 32 && N->ch <= 126)
		snprintf (buffer, sizeof(buffer), "'%c'", (int)N->ch);
	else
		snprintf (buffer, sizeof(buffer), "U+%04X", N->ch);

	Outputf ("%*s%6d : %s\n", depth * 2, "", N->weight, buffer);

	if (N->ch == HFCHAR_BRANCH)
	{
		HUFF_DumpTree (N->left,  depth + 1);
		HUFF_DumpTree (N->right, depth + 1);
	}
}

//----------------------------------------------------------------------

void HUFF_SimplifyChar (struct Node * T)
{
	char buffer[64];

	int len = tweak_utf8_encode_char (buffer, (tw_char_t) T->num);
	buffer[len] = 0;

	T->kind = NL_String;
	Node_SetString (T, buffer);
}

void HUFF_SimplifyInt (struct Node * T)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%d", T->num);

	T->kind = NL_String;
	Node_SetString (T, buffer);
}

void HUFF_SimplifyFloat (struct Node * T)
{
	char buffer[256];

	double magnitude = fabs (T->data.flt);

	if (magnitude == 0.0)
		strcpy (buffer, "0.0");
	else if (magnitude >= 0.000100 && magnitude < 1e10)
		snprintf (buffer, sizeof(buffer), "%1.6f", T->data.flt);
	else
		snprintf (buffer, sizeof(buffer), "%1.6e", T->data.flt);

	T->kind = NL_String;
	Node_SetString (T, buffer);
}

void HUFF_Simplify (struct Node * comp)
{
	struct Node * T;

	for (T = comp->children ; T != NULL ; T = T->next)
	{
		switch (T->kind)
		{
			case NL_Char:
				HUFF_SimplifyChar (T);
				break;

			case NL_Integer:
				HUFF_SimplifyInt (T);
				break;

			case NL_Float:
				HUFF_SimplifyFloat (T);
				break;

			case NL_FltSpec:
				T->kind = NL_String;
				break;

			default:
				break;
		}
	}
}

// visit all the string pseudo-instructions, simplifying complex
// strings (e.g. convert NL_Char to NL_String), converting NL_Integer
// into a string representation, and for `huffstr` analysing them all.

void HUFF_AnalyseAll (struct Node * list)
{
	struct Node * ins;

	for (ins = list->children ; ins != NULL ; ins = ins->next)
	{
		if (ins->kind != ND_String)
			continue;

		SetErrorPos (ins->pos);

		// look-up names, evaluate expressions
		if (! ExpressionsInNode (ins, ECTX_Resolve, NULL))
			continue;

		enum ObjectType obj_type = (enum ObjectType) ins->num;

		struct Node * T = ins->children;

		if (T->kind == NS_Complex)
			HUFF_Simplify (T);

		if (obj_type == OBJ_HuffString)
			HUFF_Analyse (T);
	}
}

//----------------------------------------------------------------------

void HUFF_EncodeNode (struct HuffmanNode * N)
{
	if (N->up != NULL)
	{
		HUFF_EncodeNode (N->up);
		HUFF_AddBit (huff.block, (N == N->up->right) ? 1 : 0);
	}
}

void HUFF_EncodeChar (uchar_t ch)
{
	struct HuffmanNode * N;

	if (ch < QUICK_CHARS)
		N = huff.quick[ch];
	else
		N = HUFF_FindChar (ch);

	Assert (N != NULL);

	HUFF_EncodeNode (N);
}

void HUFF_EncodeIndirect (struct Node * indirect)
{
	Assert (indirect->data.huff != NULL);

	HUFF_EncodeNode (indirect->data.huff);
}

void HUFF_EncodeStr (const char * str)
{
	uchar_t ch;

	while (*str != 0)
	{
		str += tweak_utf8_decode_char (&ch, str);	
		HUFF_EncodeChar (ch);
	}
}

void HUFF_Encode (struct Node * node)
{
	huff.block = HUFF_CreateBlock ();

	node->data.blk = huff.block;

	if (node->kind == NL_String)
	{
		HUFF_EncodeStr (Node_Str (node));
	}
	else
	{
		Assert (node->kind == NS_Complex);

		struct Node * T;
		for (T = node->children ; T != NULL ; T = T->next)
		{
			switch (T->kind)
			{
				case NL_String:
					HUFF_EncodeStr (Node_Str (T));
					break;

				case NL_Char:
					HUFF_EncodeChar ((uchar_t) T->num);
					break;

				case NS_Indirect:
					HUFF_EncodeIndirect (T);
					break;

				case NX_Address:
					// an error has already been posted, just ignore it here
					break;

				default:
					Fatal_Error ("BUG: unknown node in complex string\n");
					break;
			}
		}
	}

	// add the terminator
	HUFF_EncodeNode (huff.terminator);
}

// in this pass we visit each `huffstr` pseudo-instruction and create
// the Huffman-encoding version of its string (simple or complex).

void HUFF_EncodeAll (struct Node * list)
{
	struct Node * ins;

	for (ins = list->children ; ins != NULL ; ins = ins->next)
	{
		if (ins->kind != ND_String)
			continue;

		SetErrorPos (ins->pos);

		enum ObjectType obj_type = (enum ObjectType) ins->num;

		if (obj_type == OBJ_HuffString)
			HUFF_Encode (ins->children);
	}
}

//----------------------------------------------------------------------

void HUFF_ProcessStrings (void)
{
	Outputf ("[processing strings]\n");

	HUFF_Init ();

	// ensure there is always one non-terminator node
	HUFF_AddChar (' ');

	HUFF_AnalyseAll (stream[SECTION_TEXT]);
	HUFF_AnalyseAll (stream[SECTION_DATA]);

	if (HaveErrors ())
		return;

//	HUFF_DumpASCII ();

	HUFF_BuildTree ();

	if (huff.merged != NULL)
	{
		// HUFF_DumpTree (huff.merged, 0);

		HUFF_EncodeAll (stream[SECTION_TEXT]);
		HUFF_EncodeAll (stream[SECTION_DATA]);
	}
}

//----------------------------------------------------------------------

void HUFF_WriteIndirect (struct HuffmanNode * N, bool write_it)
{
	// WISH: support the `hilo` syntax in the arguments

	Assert (N->indirect != NULL);

	SetErrorPos (N->indirect->pos);

	if (write_it)
	{
		// determine final label addresses
		if (! ExpressionsInNode (N->indirect, ECTX_Assemble, NULL))
			return;
	}

	// handle the base...
	struct Node * base = N->indirect->children;

	bool is_double = (base->kind == NX_Memory);
	bool has_args  = (base->next != NULL);

	huff.table_pos += 1;
	huff.table_pos += 4;

	if (write_it)
	{
		int kind = (has_args ? 0x0A : 0x08) + (is_double ? 1 : 0);

		OUT_Byte (kind);

		if (base->kind == NL_Integer)
			OUT_Long (base->num);
		else
			ASM_Operand (base);
	}

	if (has_args)
	{
		if (write_it)
			OUT_Long (Node_Len (N->indirect) - 1);

		huff.table_pos += 4;
	}

	// write the arguments...
	struct Node * T;

	for (T = base->next ; T != NULL ; T = T->next)
	{
		if (write_it)
		{
			if (T->kind == NL_Integer || T->kind == NL_Char)
				OUT_Long (T->num);
			else
				ASM_Operand (T);
		}

		huff.table_pos += 4;
	}
}

uint32_t HUFF_WriteNode (struct HuffmanNode * N, bool write_it)
{
	if (! write_it)
		huff.table_nodes += 1;

	if (N->ch == HFCHAR_BRANCH)
	{
		uint32_t L_addr = HUFF_WriteNode (N->left,  write_it);
		uint32_t R_addr = HUFF_WriteNode (N->right, write_it);

		uint32_t addr = huff.table_pos;

		if (write_it)
		{
			OUT_Byte (0x00);
			OUT_Long (L_addr);
			OUT_Long (R_addr);
		}

		huff.table_pos += 9;

		return addr;
	}

	uint32_t addr = huff.table_pos;

	if (N->ch == HFCHAR_INDIRECT)
	{
		HUFF_WriteIndirect (N, write_it);
	}
	else if (N->ch == HFCHAR_TERMINATE)
	{
		if (write_it)
			OUT_Byte (0x01);

		huff.table_pos += 1;
	}
	else if (N->ch < 0x0100)  /* a Latin-1 character */
	{
		if (write_it)
		{
			OUT_Byte (0x02);
			OUT_Byte ((uint8_t) N->ch);
		}

		huff.table_pos += 2;
	}
	else  /* a Unicode character */
	{
		if (write_it)
		{
			OUT_Byte (0x04);
			OUT_Long (N->ch);
		}

		huff.table_pos += 5;
	}

	return addr;
}

uint32_t HUFF_MeasureTable (uint32_t start)
{
	// this returns the address after end of the table

	if (huff.merged == NULL)
	{
		huff.table_start = 0;
		return start;
	}

	Assert (huff.merged->ch == HFCHAR_BRANCH);

	huff.table_start = start;
	huff.table_pos   = start + (3 * 4);
	huff.table_nodes = 0;

	huff.table_root   = HUFF_WriteNode (huff.merged, false);
	huff.table_length = huff.table_pos - start;

	return huff.table_pos;
}

uint32_t HUFF_TableAddress (void)
{
	return huff.table_start;
}

void HUFF_WriteTable (void)
{
	if (huff.merged == NULL)
		return;

	OUT_Long (huff.table_length);
	OUT_Long (huff.table_nodes);
	OUT_Long (huff.table_root);

	huff.table_pos = huff.table_start + (3 * 4);
	HUFF_WriteNode (huff.merged, true);
}

uint32_t HUFF_MeasureString (struct Node * node)
{
	struct HuffBlock * blk = node->data.blk;
	Assert (blk != NULL);

	// add one for the object type byte
	return (blk->len + 7) / 8 + 1;
}

void HUFF_WriteString (struct Node * node)
{
	struct HuffBlock * blk = node->data.blk;
	Assert (blk != NULL);

	uint32_t bytes = (blk->len + 7) / 8;
	uint32_t i;

	OUT_Byte ((uint8_t) OBJ_HuffString);

	for (i = 0 ; i < bytes ; i++)
		OUT_Byte (blk->data[i]);
}
