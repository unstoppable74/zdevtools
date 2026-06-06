// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* a Node represents an element of a parsed line or file */

enum NodeKind
{
	/* low level nodes */

	NL_ERROR,     // str is the message
	NL_EOF,

	NL_Integer,   // an integer literal
	NL_Float,     // a floating-point literal
	NL_FltSpec,   // a special FP value (NAN / INFINITY)
	NL_Char,      // a character literal
	NL_String,    // a string in double quotes

	NL_Word,      // an identifier
	NL_Symbol,    // a symbol, such as `+` or `>>`
	NL_Bracket,   // an un-escaped `() [] {}` (only used in lexer)

	/* grouping nodes */

	NG_Expr,      // a group of tokens in `()` parentheses
	NG_List,      // a group of tokens in `[]` square brackets
	NG_Block,     // a group of tokens in `{}` curly brackets
	NG_Line,      // a tokenized line of tokens

	/* expressions / operands */

	NX_Address,   // an address, optional child is pending expression
	NX_JumpDest,  // a jump destination (a label)
	NX_JumpFail,  // a jump destination with `fail` (Z-code only)
	NX_Return,    // a jump destination which returns

	NX_Local,     // a local var (in the call frame)
	NX_Global,    // a global var (Z-code only)
	NX_Memory,    // a memory access operand in `[]`

	NX_Pop,       // a pop-from-stack operand
	NX_Push,      // a push-to-stack operand
	NX_Drop,      // a no-store operand
	NX_Hilo,      // split following FP constant into HI:LO parts

	NX_Unary,     // str is the operator, child is the value
	NX_Binary,    // str is the operator, children are left / right

	/* instructions */

	NI_Opcode,    // a normal instruction, children are operands
	NI_Label,     // a label
	NI_Function,  // beginning of a new function

	NI_Include,   // an include directive
	NI_Section,   // a section directive

	/* data */

	ND_Data,      // for `db`, `dw`, `dd`, etc...
	ND_Reserve,   // for `resb`, etc...
	ND_Align,     // for `align`
	ND_String,    // for `latstr`, `unistr` and `huffstr`

	/* complex strings */

	NS_Complex,   // children are NL_String, NL_Char, NS_Indirect
	NS_Indirect,  // huff_addr | zero or more values

	/* miscellanous */

	NN_Okay,      // ignore, since the parser handled it
};

struct Node
{
	enum NodeKind    kind;
	struct Position  pos;

	int              str;
	int              num;

	union
	{
		double  flt;  // NL_Float

		const struct OpcodeInfo   * gop;   // NI_Opcode (for Glulx)
		const struct Z_OpcodeInfo * zop;   // NI_Opcode (for Z-code)
		      struct LabelDef     * lab;   // NI_Label
		      struct FunctionDef  * fun;   // NI_Function
		      struct HuffmanNode  * huff;  // NS_Indirect
		      struct HuffBlock    * blk;   // ND_String
		      struct Z_TextBlock  * ztx;   // NL_String
	} data;

	struct Node    * children;
	struct Node    * tail;
	struct Node    * next;
};

/* API */

struct Node * Node_New (enum NodeKind kind, const char * str, struct Position pos);

struct Node * Node_Copy      (struct Node * T);
void          Node_Clear     (struct Node * T);
const char  * Node_Str       (struct Node * T);
void          Node_SetString (struct Node * T, const char * str);

bool          Node_Empty     (struct Node * list);
int           Node_Len       (struct Node * list);
struct Node * Node_Head      (struct Node * list);
struct Node * Node_Tail      (struct Node * list);

void          Node_Add       (struct Node * list, struct Node * T);
void          Node_AddFront  (struct Node * list, struct Node * T);
void          Node_AddAfter  (struct Node * list, struct Node * prev, struct Node * T);
struct Node * Node_Pop       (struct Node * list);
struct Node * Node_PopTail   (struct Node * list);

bool          Match          (struct Node * T, const char * keyword);
bool          MatchBracket   (struct Node * T, const char * bracket);

const char *  Node_Desc      (struct Node * T);
void          Node_Dump      (const char * msg, struct Node * T);
