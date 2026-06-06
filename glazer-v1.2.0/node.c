// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"


struct Node * Node_New (enum NodeKind kind, const char * str, struct Position pos)
{
	if (str == NULL)
		str = "";

	struct Node * T = UT_Alloc (sizeof(struct Node));

	T->kind = kind;
	T->pos  = pos;
	T->str  = STR_Intern (str);

	return T;
}

void Node_Clear (struct Node * T)
{
	T->children = NULL;
	T->tail     = NULL;
}

struct Node * Node_Copy (struct Node * T)
{
	struct Node * copy = UT_Alloc (sizeof(struct Node));

	memcpy (copy, T, sizeof(struct Node));

	// children are NOT copied
	copy->children = NULL;
	copy->tail     = NULL;
	copy->next     = NULL;

	return copy;
}

const char * Node_Str (struct Node * T)
{
	return STR_Get (T->str);
}

void Node_SetString (struct Node * T, const char * str)
{
	if (str == NULL)
		str = "";

	T->str = STR_Intern (str);
}

//----------------------------------------------------------------------

bool Node_Empty (struct Node * list)
{
	return (list->children == NULL);
}

int Node_Len (struct Node * list)
{
	int count = 0;
	struct Node * child;

	for (child = list->children ; child != NULL ; child = child->next)
		count += 1;

	return count;
}

struct Node * Node_Head (struct Node * list)
{
	return list->children;
}

struct Node * Node_Tail (struct Node * list)
{
	return list->tail;
}

void Node_Add (struct Node * list, struct Node * elem)
{
	elem->next = NULL;

	if (list->tail != NULL)
		list->tail->next = elem;
	else
		list->children = elem;

	list->tail = elem;
}

void Node_AddFront (struct Node * list, struct Node * elem)
{
	elem->next = list->children;

	list->children = elem;

	if (list->tail == NULL)
		list->tail = elem;
}

void Node_AddAfter (struct Node * list, struct Node * prev, struct Node * elem)
{
	if (prev->next == NULL)
	{
		Node_Add (list, elem);
		return;
	}

	elem->next = prev->next;
	prev->next = elem;
}

struct Node * Node_Pop (struct Node * list)
{
	struct Node * head = list->children;

	if (head == NULL)
		Fatal_Error ("Node_Pop: no children\n");

	list->children = head->next;

	if (list->children == NULL)
		list->tail = NULL;

	return head;
}

struct Node * Node_PopTail (struct Node * list)
{
	struct Node * tail = list->tail;

	if (tail == NULL)
		Fatal_Error ("Node_PopTail: no children\n");

	if (tail == list->children)
	{
		list->children = NULL;
		list->tail     = NULL;

		return tail;
	}

	// find the new tail
	struct Node * cur = list->children;

	while (cur->next != tail)
		cur = cur->next;

	cur->next  = NULL;
	list->tail = cur;

	return tail;
}

//----------------------------------------------------------------------

bool Match (struct Node * T, const char * keyword)
{
	switch (T->kind)
	{
		case NL_Word:
		case NL_Symbol:
			return (strcmp (Node_Str (T), keyword) == 0);

		default:
			return false;
	}
}

bool MatchBracket (struct Node * T, const char * bracket)
{
	if (T->kind != NL_Bracket)
		return false;

	return (strcmp (Node_Str (T), bracket) == 0);
}

//----------------------------------------------------------------------

// this returns something usable in error messages,
// especially ones of the form "expected xxx, got yyy".
const char * Node_Desc (struct Node * T)
{
	static char buffer[256];

	const char * str = Node_Str (T);

	switch (T->kind)
	{
		case NL_EOF:      return "<EOF>";
		case NL_ERROR:    return "<ERROR>";

		case NL_Integer:  return "an integer";
		case NL_Float:    return "a float";
		case NL_Char:     return "a character";
		case NL_String:   return "a string";

		case NL_FltSpec:
			snprintf (buffer, sizeof(buffer), "the constant '%s'", str);
			return buffer;

		case NL_Word:
			snprintf (buffer, sizeof(buffer), "the word '%s'", str);
			return buffer;

		case NL_Symbol:
		case NL_Bracket:
			snprintf (buffer, sizeof(buffer), "the symbol '%s'", str);
			return buffer;

		case NG_Expr:     return "stuff in ()";
		case NG_List:     return "stuff in []";
		case NG_Block:    return "stuff in {}";
		case NG_Line:     return "a raw line";

		default:          return "something odd";
	}
}

//----------------------------------------------------------------------

/* debugging stuff... */

const char * Node_EncodeStr (struct Node * T)
{
	const char * str = Node_Str (T);

	static char buffer[256];
	size_t      b_pos = 0;

	buffer[b_pos++] = '"';

	while (*str != 0 && b_pos < 50)
	{
		uint8_t ch = (uint8_t) *str++;

		// escape control chars
		if (ch < 32 || ch == 127 || ch == '"' || ch == '\\')
		{
			buffer[b_pos++] = '\\';
			buffer[b_pos++] = '0' + (ch >> 6);
			buffer[b_pos++] = '0' + ((ch >> 3) & 7);
			buffer[b_pos++] = '0' + (ch & 7);
		}
		else
		{
			buffer[b_pos++] = (char) ch;
		}
	}

	if (*str != 0)
	{
		buffer[b_pos++] = '.';
		buffer[b_pos++] = '.';
		buffer[b_pos++] = '.';
	}

	buffer[b_pos++] = '"';
	buffer[b_pos++] = 0;

	return buffer;
}

const char * Node_InfoString (struct Node * T)
{
	if (T == NULL)
		return "NULL";

	static char buffer[256];

	const char * s = Node_EncodeStr (T);

	switch (T->kind)
	{
		/* low level nodes */

		case NL_ERROR:
			snprintf (buffer, sizeof(buffer), "ERROR %s", s);
			return buffer;

		case NL_EOF:
			return "EOF";

		case NL_Integer:
			snprintf (buffer, sizeof(buffer), "NL_Integer %d", T->num);
			return buffer;

		case NL_Float:
			snprintf (buffer, sizeof(buffer), "NL_Float %1.9f", T->data.flt);
			return buffer;

		case NL_FltSpec:
			snprintf (buffer, sizeof(buffer), "NL_FltSpec %s", s);
			return buffer;

		case NL_Char:
			snprintf (buffer, sizeof(buffer), "NL_Char U+%04X", T->num);
			return buffer;

		case NL_String:
			snprintf (buffer, sizeof(buffer), "NL_String %s", s);
			return buffer;

		case NL_Word:
			snprintf (buffer, sizeof(buffer), "NL_Word %s", s);
			return buffer;

		case NL_Symbol:
			snprintf (buffer, sizeof(buffer), "NL_Symbol %s", s);
			return buffer;

		case NL_Bracket:
			snprintf (buffer, sizeof(buffer), "NL_Bracket %s", s);
			return buffer;

		/* grouping nodes */

		case NG_Expr:     return "NG_Expr";
		case NG_List:     return "NG_List";
		case NG_Block:    return "NG_Block";
		case NG_Line:     return "NG_Line";

		/* expressions */

		case NX_Address:
			snprintf (buffer, sizeof(buffer), "NX_Address %s", s);
			return buffer;

		case NX_JumpDest:
			snprintf (buffer, sizeof(buffer), "NX_JumpDest (pc $%04X)", T->num);
			return buffer;

		case NX_JumpFail:
			snprintf (buffer, sizeof(buffer), "NX_JumpFail (pc $%04X)", T->num);
			return buffer;

		case NX_Return:
			snprintf (buffer, sizeof(buffer), "NX_Return (%d)", T->num);
			return buffer;

		case NX_Local:
			snprintf (buffer, sizeof(buffer), "NX_Local %s (%+d)", s, T->num);
			return buffer;

		case NX_Global:
			snprintf (buffer, sizeof(buffer), "NX_Global %s (#%d)", s, T->num);
			return buffer;

		case NX_Memory:   return "NX_Memory";

		case NX_Pop:      return "NX_Pop";
		case NX_Push:     return "NX_Push";
		case NX_Drop:     return "NX_Drop";
		case NX_Hilo:     return "NX_Hilo";

		case NX_Unary:
			snprintf (buffer, sizeof(buffer), "NX_Unary %s", s);
			return buffer;

		case NX_Binary:
			snprintf (buffer, sizeof(buffer), "NX_Binary %s", s);
			return buffer;

		/* instructions */

		case NI_Opcode:
			snprintf (buffer, sizeof(buffer), "NI_Opcode %s", s);
			return buffer;

		case NI_Label:
			snprintf (buffer, sizeof(buffer), "NI_Label %s", s);
			return buffer;

		case NI_Function:
			return "NI_Function";

		case NI_Include:
			snprintf (buffer, sizeof(buffer), "NI_Include %s", s);
			return buffer;

		case NI_Section:
			snprintf (buffer, sizeof(buffer), "NI_Section %s", s);
			return buffer;

		/* data */

		case ND_Data:       return "ND_Data";
		case ND_Reserve:    return "ND_Reserve";
		case ND_Align:      return "ND_Align";
		case ND_String:     return "ND_String";

		/* complex strings */

		case NS_Complex:    return "NS_Complex";
		case NS_Indirect:   return "NS_Indirect";

		/* miscellaneous */

		case NN_Okay:       return "NN_Okay";

		default: break;
	}

	return "??UNKNOWN??";
}

void Node_DoDump (struct Node * T, int indent)
{
	Outputf ("%*s%s\n", indent, "", Node_InfoString (T));

	struct Node * child;

	for (child = T->children ; child != NULL ; child = child->next)
		Node_DoDump (child, indent + 3);
}

void Node_Dump (const char * msg, struct Node * T)
{
	if (msg != NULL)
		Outputf ("%s\n", msg);

	Node_DoDump (T, 3);
}
