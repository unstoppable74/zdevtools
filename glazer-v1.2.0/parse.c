// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

struct Node * P_FAIL;
struct Node * P_OKAY;
struct Node * P_UNKNOWN;

static struct FunctionDef * last_function;

void P_InitParser (void)
{
	P_FAIL    = Node_New (NL_ERROR, "PARSE FAILURE", no_position);
	P_OKAY    = Node_New (NN_Okay,  "PARSE SUCCESS", no_position);
	P_UNKNOWN = Node_New (NL_ERROR, "PARSE UNKNOWN", no_position);
}

//----------------------------------------------------------------------

struct Node * P_TryParseLabel (struct Node * input)
{
	if (Node_Len (input) < 2)
		return NULL;

	struct Node * T = input->children;
	struct Node * U = T->next;

	if (T->kind != NL_Word)
		return NULL;

	if (! Match (U, ":"))
		return NULL;

	Node_Pop (input);
	Node_Pop (input);

	T->kind = NI_Label;

	/* handle private labels */

	const char * name = Node_Str (T);
	if (name[0] == '.')
	{
		if (last_function == NULL)
		{
			PostError ("private label '%s' is outside of a function", name);
			return P_FAIL;
		}

		if (F_FindPrivateLabel (last_function, name) != NULL)
		{
			PostError ("private label '%s' already exists", name);
			return P_FAIL;
		}

		T->data.lab = F_AddPrivateLabel (last_function, name);
	}
	else  /* handle normal labels */
	{
		if (! ValidateName (name, "label", false))
			return P_FAIL;

		T->data.lab = DefineLabel (name);
	}

	return T;
}

//----------------------------------------------------------------------

struct Node * P_ParseConstDef    (struct Node * input, struct Node * head);
struct Node * P_ParseGlobal      (struct Node * input);
struct Node * P_ParseInclude     (struct Node * input);
struct Node * P_ParseSection     (struct Node * input);
struct Node * P_ParseRelease     (struct Node * input);
struct Node * P_ParseFunction    (struct Node * input,  enum ObjectType obj_type);
struct Node * P_ParseLocal       (struct Node * input);
struct Node * P_ParseData        (struct Node * input, int elem_size);
struct Node * P_ParseReserve     (struct Node * input, int elem_size);
struct Node * P_ParseAlign       (struct Node * input);
struct Node * P_ParseString      (struct Node * input, enum ObjectType obj_type);
struct Node * P_ParseInstruct_G  (struct Node * input, const struct OpcodeInfo * info);
struct Node * P_ParseInstruct_Z  (struct Node * input, const struct Z_OpcodeInfo * info);

struct Node * P_ParseOperand     (struct Node * input, bool is_store, const char * op_name, bool allow_string);
struct Node * P_ParseMemory      (struct Node * input);
struct Node * P_ParseJumpDest    (struct Node * input);

struct Node * P_ParseExpression  (struct Node * input, bool allow_string);
struct Node * P_ParseExprPart    (struct Node * input, struct Node * lhs, int min_prec, bool allow_string);
struct Node * P_ParseTerm        (struct Node * input, bool allow_string);
struct Node * P_ParseComplexStr  (struct Node * input);

struct Node * P_ParseValue       (struct Node * T, bool allow_string);


// the main function for parsing assembly code.
// this will usually return a specific node, like `NI_Opcode` for a
// normal instruction.  it may also return `P_FAIL` on an error, and
// `P_OKAY` if it could handle the thing itself.
struct Node * P_ParseLine (struct Node * input)
{
	Assert (! Node_Empty (input));

	struct Node * head = Node_Pop (input);

	SetErrorPos (head->pos);

	if (head->kind != NL_Word)
	{
		PostError ("expected a word, got %s", Node_Desc (head));
		return P_FAIL;
	}

	if (! Node_Empty (input))
	{
		if (Match (input->children, "="))
			return P_ParseConstDef (input, head);
	}

	/* directives */

	if (Match (head, "include")) return P_ParseInclude (input);
	if (Match (head, "section")) return P_ParseSection (input);
	if (Match (head, "release")) return P_ParseRelease (input);
	if (Match (head, "global"))  return P_ParseGlobal  (input);

	/* function stuff */

	if (Match (head, "function")) return P_ParseFunction (input, OBJ_Function);
	if (Match (head, "func_va"))  return P_ParseFunction (input, OBJ_FuncVA);
	if (Match (head, "local"))    return P_ParseLocal    (input);

	/* data stuff */

	if (Match (head, "db")) return P_ParseData (input, 1);
	if (Match (head, "dw")) return P_ParseData (input, 2);
	if (Match (head, "dd")) return P_ParseData (input, 4);
	if (Match (head, "dq")) return P_ParseData (input, 8);

	if (Match (head, "resb")) return P_ParseReserve (input, 1);
	if (Match (head, "resw")) return P_ParseReserve (input, 2);
	if (Match (head, "resd")) return P_ParseReserve (input, 4);
	if (Match (head, "resq")) return P_ParseReserve (input, 8);

	if (Match (head, "align")) return P_ParseAlign (input);

	if (! options.zcode)
	{
		if (Match (head, "latstr"))  return P_ParseString (input, OBJ_LatinString);
		if (Match (head, "huffstr")) return P_ParseString (input, OBJ_HuffString);
		if (Match (head, "unistr"))  return P_ParseString (input, OBJ_UnicodeString);
	}
	else
	{
		// TODO: Z-code strings (`zstring` or `packstr`)
	}

	/* anything else must be an instruction */

	const char * name = Node_Str (head);

	if (! options.zcode)
	{
		const struct OpcodeInfo * info = LookupOp (name);
		if (info != NULL)
			return P_ParseInstruct_G (input, info);
	}
	else
	{
		const struct Z_OpcodeInfo * info = Z_LookupOp (name, false);
		if (info != NULL)
			return P_ParseInstruct_Z (input, info);

		// was it for a different version of the Z-machine?
		info = Z_LookupOp (name, true);
		if (info != NULL)
		{
			PostError ("instruction '%s' not available in z%d", name, z_version);
			return P_FAIL;
		}
	}

	PostError ("unknown directive or instruction '%s'", name);
	return P_FAIL;
}

//----------------------------------------------------------------------

struct Node * P_ParseConstDef (struct Node * input, struct Node * head)
{
	// remove equals sign
	Node_Pop (input);

	if (Node_Empty (input))
	{
		PostError ("missing value after '='");
		return P_FAIL;
	}

	struct Node * V = P_ParseExpression (input, false);
	if (V == P_FAIL)
		return P_FAIL;

	const char * name = Node_Str (head);

	if (! ValidateName (name, "constant", false))
		return P_FAIL;

	// add to symbol table
	DefineConstant (name, V);

	return P_OKAY;
}

struct Node * P_ParseGlobal (struct Node * input)
{
	if (! options.zcode)
	{
		PostError ("cannot use 'global' directive in Glulx");
		return P_FAIL;
	}

	if (Node_Empty (input))
	{
		PostError ("missing name for global var");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	if (T->kind != NL_Word)
	{
		PostError ("global var name must be a word, got %s", Node_Desc (T));
		return P_FAIL;
	}

	const char * name = Node_Str (T);

	if (! ValidateName (name, "global var", false))
		return P_FAIL;

	// handle the value, which is optional
	struct Node * V = NULL;

	if (! Node_Empty (input))
	{
		T = Node_Pop (input);

		if (! Match (T, "="))
		{
			PostError ("expected '=' after global var name, got %s", Node_Desc (T));
			return P_FAIL;
		}

		if (Node_Empty (input))
		{
			PostError ("missing value after '='");
			return P_FAIL;
		}

		V = P_ParseExpression (input, false);
		if (V == P_FAIL)
			return P_FAIL;
	}

	// add to symbol table
	DefineGlobal (name, V);

	return P_OKAY;
}

struct Node * P_ParseInclude (struct Node * input)
{
	if (Node_Empty (input))
	{
		PostError ("missing include filename");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	if (T->kind != NL_String)
	{
		PostError ("include filename must be a string, got %s", Node_Desc (T));
		return P_FAIL;
	}

	if (! Node_Empty (input))
	{
		PostError ("extra rubbish after include filename");
		return P_FAIL;
	}

	T->kind = NI_Include;
	return T;
}

struct Node * P_ParseSection (struct Node * input)
{
	if (Node_Empty (input))
	{
		PostError ("missing section type");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	if (T->kind != NL_Word)
	{
		PostError ("section type must be a word, got %s", Node_Desc (T));
		return P_FAIL;
	}

	if (! Node_Empty (input))
	{
		PostError ("extra rubbish after section type");
		return P_FAIL;
	}

	const char * name = Node_Str (T);

	if (strcmp (name, ".text") == 0)
		T->num = (int)SECTION_TEXT;
	else if (strcmp (name, ".data") == 0)
		T->num = (int)SECTION_DATA;
	else if (strcmp (name, ".bss") == 0)
		T->num = (int)SECTION_BSS;
	else if (strcmp (name, ".static") == 0)
		T->num = (int)SECTION_STATIC;
	else
	{
		PostError ("unknown section (not .text .data .bss .static)");
		return P_FAIL;
	}

	if (! options.zcode && T->num == (int)SECTION_STATIC)
	{
		PostError ("cannot use .static section in Glulx code");
		return P_FAIL;
	}
	else if (options.zcode && T->num == (int)SECTION_BSS)
	{
		PostError ("cannot use .bss section in Z-code");
		return P_FAIL;
	}

	T->kind = NI_Section;
	return T;
}

struct Node * P_ParseRelease (struct Node * input)
{
	if (! options.zcode)
	{
		PostError ("cannot use 'release' directive in Glulx");
		return P_FAIL;
	}

	// the syntax is `release NUMBER [ SERIAL ]`.
	// the number is compulsory, the serial string is optional.

	if (Node_Empty (input))
	{
		PostError ("missing release number");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	if (T->kind != NL_Integer)
	{
		PostError ("release number must be an integer, got %s", Node_Desc (T));
		return P_FAIL;
	}

	int number = T->num;
	const char * serial = NULL;

	if (! Node_Empty (input))
	{
		T = Node_Pop (input);

		if (T->kind != NL_String)
		{
			PostError ("serial number must be a string, got %s", Node_Desc (T));
			return P_FAIL;
		}

		serial = Node_Str (T);
	}

	if (! AssembleSetRelease (number, serial))
		return P_FAIL;

	return P_OKAY;
}

struct Node * P_ParseFunction (struct Node * input, enum ObjectType obj_type)
{
	if (! Node_Empty (input))
	{
		PostError ("extra rubbish after function keyword");
		return P_FAIL;
	}

	struct FunctionDef * fun = NewFunction ();

	struct Node * T = Node_New (NI_Function, "", input->pos);
	T->num      = (int)obj_type;
	T->data.fun = fun;

	last_function = fun;
	return T;
}

struct Node * P_ParseLocal (struct Node * input)
{
	if (Node_Empty (input))
	{
		PostError ("missing name for local var");
		return P_FAIL;
	}

	if (last_function == NULL)
	{
		PostError ("cannot use 'local' outside of a function");
		return P_FAIL;
	}

	while (! Node_Empty (input))
	{
		struct Node * T = Node_Pop (input);

		if (T->kind != NL_Word)
		{
			PostError ("local name must be a word, got %s", Node_Desc (T));
			return P_FAIL;
		}

		const char * name = Node_Str (T);

		if (! ValidateName (name, "local var", true))
			return P_FAIL;

		if (F_FindLocalVar (last_function, name) != NULL)
		{
			PostError ("local variable '%s' already exists", name);
			return P_FAIL;
		}

		F_AddLocalVar (last_function, name);
	}

	return P_OKAY;
}

struct Node * P_ParseData (struct Node * input, int elem_size)
{
	if (Node_Empty (input))
	{
		PostError ("missing data elements");
		return P_FAIL;
	}

	struct Node * data = Node_New (ND_Data, "", input->pos);
	data->num = elem_size;

	while (! Node_Empty (input))
	{
		struct Node * T = Node_Pop (input);
		struct Node * V = P_ParseValue (T, true /* allow_string */);

		if (V == P_FAIL)
			return P_FAIL;

		Node_Add (data, V);
	}

	if (Node_Empty (data))
	{
		PostError ("missing data elements");
		return P_FAIL;
	}

	return data;
}

struct Node * P_ParseReserve (struct Node * input, int elem_size)
{
	if (Node_Empty (input))
	{
		PostError ("missing reserve count");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);
	struct Node * V = P_ParseValue (T, false);

	if (V == P_FAIL)
		return P_FAIL;

	if (! Node_Empty (input))
	{
		PostError ("extra rubbish after reserve count");
		return P_FAIL;
	}

	T = Node_New (ND_Reserve, "", input->pos);
	T->num = elem_size;

	Node_Add (T, V);
	return T;
}

struct Node * P_ParseAlign (struct Node * input)
{
	if (Node_Empty (input))
	{
		PostError ("missing value after 'align'");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);
	struct Node * V = P_ParseValue (T, false);

	if (V == P_FAIL)
		return P_FAIL;

	if (! Node_Empty (input))
	{
		PostError ("extra rubbish after alignment value");
		return P_FAIL;
	}

	T = Node_New (ND_Align, "", input->pos);
	Node_Add (T, V);
	return T;
}

struct Node * P_ParseString (struct Node * input, enum ObjectType obj_type)
{
	if (Node_Empty (input))
	{
		PostError ("missing string");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);
	struct Node * V = P_ParseValue (T, true /* allow_string */);

	if (V == P_FAIL)
		return P_FAIL;

	if (obj_type != OBJ_HuffString && V->kind == NS_Complex && V->num > 0)
	{
		SetErrorPos (V->pos);
		PostError ("cannot use indirect strings with '%s'",
			(obj_type == OBJ_LatinString) ? "latstr" : "unistr");
		return P_FAIL;
	}

	T = Node_New (ND_String, "", input->pos);
	T->num = (int)obj_type;

	Node_Add (T, V);
	return T;
}

//----------------------------------------------------------------------

struct Node * P_ParseInstruct_G (struct Node * input, const struct OpcodeInfo * info)
{
	struct Node * ins = Node_New (NI_Opcode, info->name, input->pos);

	ins->data.gop = info;

	bool is_jump  = ((info->flags & OPFLAG_JUMP) != 0);
	bool is_catch = (info->op == OP_CATCH);

	int num_load  = 0;
	int num_store = 0;

	// allow a missing value for `return` instruction
	if (info->op == OP_RETURN && Node_Empty (input))
	{
		struct Node * T = Node_New (NL_Integer, "", input->pos);
		T->num = 0;
		Node_Add (input, T);
	}

	/* parse the load operands */

	for (; num_load < info->loads ; num_load++)
	{
		if (Node_Empty (input) || Match (input->children, "->"))
		{
			PostError ("missing load operand in '%s' instruction", info->name);
			return P_FAIL;
		}

		struct Node * T = P_ParseOperand (input, false, info->name, false);
		if (T == P_FAIL)
			return P_FAIL;

		Node_Add (ins, T);
	}

	/* parse the store operands (they are optional) */

	if (info->stores > 0 && ! Node_Empty (input) && Match (input->children, "->"))
	{
		// skip the `->`
		Node_Pop (input);

		for (; num_store < info->stores ; num_store++)
		{
			if (Node_Empty (input))
			{
				PostError ("missing store operand in '%s' instruction", info->name);
				return P_FAIL;
			}

			if (Match (input->children, "->"))
			{
				PostError ("unexpected '->' in '%s' instruction", info->name);
				return P_FAIL;
			}

			struct Node * T = P_ParseOperand (input, true, info->name, false);
			if (T == P_FAIL)
				return P_FAIL;

			Node_Add (ins, T);
		}
	}
	else
	{
		// when stores are absent, they all become "drop"

		for (; num_store < info->stores ; num_store++)
		{
			struct Node * T = Node_New (NX_Drop, "", input->pos);
			Node_Add (ins, T);
		}
	}

	/* destination of a jump */

	if (is_jump || is_catch)
	{
		if (Node_Empty (input))
		{
			PostError ("missing jump target in '%s' instruction", info->name);
			return P_FAIL;
		}

		// the arrow is optional for the plain `jump` instruction
		if (Match (input->children, "->"))
		{
			// skip the `->`
			Node_Pop (input);
		}
		else if (info->op != OP_JUMP)
		{
			PostError ("missing '->' in '%s' instruction", info->name);
			return P_FAIL;
		}

		if (Node_Empty (input))
		{
			PostError ("missing jump target in '%s' instruction", info->name);
			return P_FAIL;
		}

		struct Node * T = P_ParseJumpDest (input);
		if (T == P_FAIL)
			return P_FAIL;

		Node_Add (ins, T);
	}

	if (! Node_Empty (input))
	{
		PostError ("too many operands to '%s' instruction", info->name);
		return P_FAIL;
	}

	// handle the fake "push" and "pop" instructions
	if (info->op == OP_FAKE_PUSH || info->op == OP_FAKE_PULL)
	{
		Node_SetString (ins, "copy");
		ins->data.gop = LookupOp ("copy");

		if (info->op == OP_FAKE_PUSH)
			Node_Add (ins, Node_New (NX_Push, "", ins->pos));
		else
			Node_AddFront (ins, Node_New (NX_Pop, "", ins->pos));
	}

	return ins;
}

struct Node * P_ParseInstruct_Z (struct Node * input, const struct Z_OpcodeInfo * info)
{
	struct Node * ins = Node_New (NI_Opcode, info->name, input->pos);

	ins->data.zop = info;

	bool is_jump  = ((info->flags & OPFLAG_JUMP)   != 0);
	bool is_store = ((info->flags & OPFLAG_ZSTORE) != 0);
//	bool is_var   = ((info->flags & OPFLAG_ZVAR)   != 0);
//	bool is_call  = ((info->flags & OPFLAG_ZCALL)  != 0);
	bool is_text  = ((info->flags & OPFLAG_ZTEXT)  != 0);

	// allow a missing value for `ret` instruction
	if (info->op == ZOP_RET && Node_Empty (input))
	{
		struct Node * T = Node_New (NL_Integer, "", input->pos);
		T->num = 0;
		Node_Add (input, T);
	}

	/* parse the load operands */

	int num_load = 0;

	for (;;)
	{
		if (Node_Empty (input) || Match (input->children, "->") || Match (input->children, "fail"))
			break;

		struct Node * T = P_ParseOperand (input, false, info->name, is_text);
		if (T == P_FAIL)
			return P_FAIL;

		// TODO: support complex strings
		if (is_text && T->kind != NL_String)
		{
			PostError ("expected string operand for '%s' (got %s)",
			           info->name, Node_Desc (T));
			return P_FAIL;
		}

		Node_Add (ins, T);
		num_load += 1;
	}

	if (num_load < info->arg_min)
	{
		PostError ("not enough load operands for '%s' (got %d, needed %d)",
		           info->name, num_load, info->arg_min);
		return P_FAIL;
	}
	else if (num_load > info->arg_max)
	{
		PostError ("too many load operands for '%s' (got %d, max is %d)",
		           info->name, num_load, info->arg_max);
		return P_FAIL;
	}

	/* parse any store operand */

	if (is_store)
	{
		if (Node_Empty (input) || ! Match (input->children, "->"))
		{
			PostError ("missing store operand in '%s' instruction", info->name);
			return P_FAIL;
		}

		// skip the `->`
		Node_Pop (input);

		if (Node_Empty (input))
		{
			PostError ("missing store operand in '%s' instruction", info->name);
			return P_FAIL;
		}
		else
		if (Match (input->children, "->"))
		{
			PostError ("unexpected '->' in '%s' instruction", info->name);
			return P_FAIL;
		}

		struct Node * T = P_ParseOperand (input, true, info->name, false);
		if (T == P_FAIL)
			return P_FAIL;

		Node_Add (ins, T);
	}

	/* destination of a jump */

	if (is_jump)
	{
		bool fail_mode = false;

		if (! Node_Empty (input) && Match (input->children, "fail"))
		{
			fail_mode = true;
			Node_Pop (input);
		}

		if (Node_Empty (input))
		{
			PostError ("missing jump target in '%s' instruction", info->name);
			return P_FAIL;
		}

		// the arrow is optional for the plain `jump` instruction
		if (Match (input->children, "->"))
		{
			// skip the `->`
			Node_Pop (input);
		}
		else if (info->op != ZOP_JUMP)
		{
			PostError ("missing '->' in '%s' instruction", info->name);
			return P_FAIL;
		}

		if (Node_Empty (input))
		{
			PostError ("missing jump target in '%s' instruction", info->name);
			return P_FAIL;
		}

		struct Node * T = P_ParseJumpDest (input);
		if (T == P_FAIL)
			return P_FAIL;

		if (fail_mode)
			T->kind = NX_JumpFail;

		Node_Add (ins, T);
	}

	return ins;
}

//----------------------------------------------------------------------

struct Node * P_ParseOperand (struct Node * input, bool is_store, const char * op_name, bool allow_string)
{
	if (Node_Empty (input))
	{
		PostError ("missing operand in '%s' instruction", op_name);
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	if (! is_store)
	{
		if (Match (T, "pop"))
		{
			T->kind = NX_Pop;
			return T;
		}
		else if (Match (T, "hilo"))
		{
			if (options.zcode)
			{
				PostError ("cannot use 'hilo' in Z-code");
				return P_FAIL;
			}

			T->kind = NX_Hilo;
			return T;
		}
	}
	else
	{
		if (Match (T, "push"))
		{
			T->kind = NX_Push;
			return T;
		}
		else if (Match (T, "drop"))
		{
			if (options.zcode)
			{
				PostError ("cannot use 'drop' in Z-code");
				return P_FAIL;
			}

			T->kind = NX_Drop;
			return T;
		}
	}

	if (Match (T, "rfalse") || Match (T, "rtrue")  ||
	    Match (T, "push")   || Match (T, "pop")    ||
	    Match (T, "drop")   || Match (T, "hilo"))
	{
		PostError ("cannot use '%s' in that context", Node_Str (T));
		return P_FAIL;
	}

	if (T->kind == NG_List)
	{
		if (options.zcode)
		{
			PostError ("cannot use '[...]' in Z-code");
			return P_FAIL;
		}

		return P_ParseMemory (T);
	}

	struct Node * V = P_ParseValue (T, allow_string);
	return V;
}

struct Node * P_ParseMemory (struct Node * input)
{
	if (Node_Empty (input))
	{
		PostError ("missing address in []");
		return P_FAIL;
	}

	struct Node * V = P_ParseExpression (input, false);
	if (V == P_FAIL)
		return P_FAIL;

	struct Node * T = Node_New (NX_Memory, "", input->pos);

	Node_Add (T, V);
	return T;
}

struct Node * P_ParseJumpDest (struct Node * input)
{
	// the caller must check for a missing expression
	Assert (! Node_Empty (input));

	struct Node * T = Node_Pop (input);

	if (Match (T, "push") ||
	    Match (T, "pop")  ||
	    Match (T, "drop"))
	{
		PostError ("cannot use '%s' as a jump target", Node_Str (T));
		return P_FAIL;
	}

	// check for two special values...
	if (Match (T, "rfalse"))
	{
		T->kind = NX_Return;
		T->num  = 0;
		return T;
	}

	if (Match (T, "rtrue"))
	{
		T->kind = NX_Return;
		T->num  = 1;
		return T;
	}

	// anything else must be a label name
	if (T->kind != NL_Word)
	{
		PostError ("expected a jump target, got %s", Node_Desc (T));
		return P_FAIL;
	}

	struct Node * jump = Node_New (NX_JumpDest, "", T->pos);
	Node_Add (jump, T);

	return jump;
}

//----------------------------------------------------------------------

struct Node * P_ParseExpression (struct Node * input, bool allow_string)
{
	if (Node_Empty (input))
	{
		PostError ("missing expression in ()");
		return P_FAIL;
	}

	struct Node * lhs = P_ParseTerm (input, allow_string);
	if (lhs == P_FAIL)
		return P_FAIL;

	struct Node * expr = P_ParseExprPart (input, lhs, 1, allow_string);
	if (expr == P_FAIL)
		return P_FAIL;

	if (! Node_Empty (input))
	{
		PostError ("bad expression in ()");
		return P_FAIL;
	}

	return expr;
}

struct Node * P_ParseExprPart (struct Node * input, struct Node * lhs, int min_prec, bool allow_string)
{
	// here is a classic "operator-precedence parser"

	for (;;)
	{
		if (Node_Empty (input))
			return lhs;

		struct Node * T = input->children;
		int prec = IsBinaryOperator (T);

		if (prec < min_prec)
			return lhs;

		Node_Pop (input);

		struct Node * rhs = P_ParseTerm (input, allow_string);
		if (rhs == P_FAIL)
			return P_FAIL;

		// check for a higher precedence operator
		for (;;)
		{
			if (Node_Empty (input))
				break;

			struct Node * U = input->children;
			int u_prec = IsBinaryOperator (U);

			if (u_prec <= prec)
				break;

			rhs = P_ParseExprPart (input, rhs, u_prec, allow_string);
		}

		T->kind = NX_Binary;
		T->num  = prec;

		Node_Add (T, lhs);
		Node_Add (T, rhs);

		lhs = T;
	}
}

struct Node * P_ParseTerm (struct Node * input, bool allow_string)
{
	if (Node_Empty (input))
	{
		PostError ("missing a term in expression");
		return P_FAIL;
	}

	struct Node * T = Node_Pop (input);

	int prec = IsUnaryOperator (T);

	if (prec > 0)
	{
		struct Node * value = P_ParseTerm (input, allow_string);
		if (value == P_FAIL)
			return P_FAIL;

		T->kind = NX_Unary;
		T->num  = prec;

		Node_Add (T, value);
		return T;
	}

	return P_ParseValue (T, allow_string);
}

//----------------------------------------------------------------------

struct Node * P_ParseValue (struct Node * T, bool allow_string)
{
	if (! allow_string && (T->kind == NL_String || T->kind == NG_Block))
	{
		PostError ("cannot use a string in that context");
		return P_FAIL;
	}

	if (T->kind == NG_List)
	{
		PostError ("cannot use [] in that context");
		return P_FAIL;
	}

	switch (T->kind)
	{
		case NL_Integer:
		case NL_Char:
		case NL_String:
			return T;

		case NL_Float:
		case NL_FltSpec:
			if (options.zcode)
			{
				PostError ("cannot use floating point in Z-code");
				return P_FAIL;
			}
			return T;

		// identifiers are resolved later (no look-up here)
		case NL_Word:
			return T;

		case NG_Expr:
			return P_ParseExpression (T, allow_string);

		case NG_Block:
			return P_ParseComplexStr (T);

		default:
			PostError ("expected a value, got %s", Node_Desc (T));
			return P_FAIL;
	}
}

//----------------------------------------------------------------------

struct Node * P_ParseIndirectStr (struct Node * input)
{
	struct Node * indy = Node_New (NS_Indirect, "", input->pos);

	while (! Node_Empty (input))
	{
		struct Node * T = Node_Pop (input);
		struct Node * V = NULL;

		// first element may be a double indirect in `[]`
		if (T->kind == NG_List && Node_Empty (indy))
			V = P_ParseMemory (T);
		else
			V = P_ParseValue (T, false);

		if (V == P_FAIL)
			return P_FAIL;

		Node_Add (indy, V);
	}

	Assert (! Node_Empty (indy));
	return indy;
}

struct Node * P_ParseComplexStr (struct Node * input)
{
	struct Node * result = Node_New (NS_Complex, "", input->pos);

	while (! Node_Empty (input))
	{
		struct Node * T = Node_Pop (input);
		struct Node * V = NULL;

		SetErrorPos (T->pos);

		switch (T->kind)
		{
			case NL_String:
			case NL_Char:
			case NL_Integer:
			case NL_Float:
			case NL_FltSpec:
				// we simply add these as-is
				Node_Add (result, T);
				break;

			case NL_Word:
				// identifiers are resolved later (no look-up here)
				Node_Add (result, T);
				break;

			case NG_Expr:
				V = P_ParseExpression (T, true);
				if (V == P_FAIL)
					return P_FAIL;

				Node_Add (result, V);
				break;

			case NG_List:
				V = P_ParseIndirectStr (T);
				if (V == P_FAIL)
					return P_FAIL;

				Node_Add (result, V);

				// mark the parent node as having an indirect string
				result->num = 1;
				break;

			case NL_Symbol:
				PostError ("unexpected symbol '%s' in complex string", T->str);
				return P_FAIL;

			default:
				PostError ("bad syntax in complex string, got: %s", Node_Desc (T));
				return P_FAIL;
		}
	}

	Assert (! Node_Empty (result));
	return result;
}
