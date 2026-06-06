// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

struct SymbolTable * all_symbols;

int num_global_vars;


void InitDefinitions (void)
{
	all_symbols = SYM_New ();

	num_global_vars = 0;
}

struct ConstantDef * NewConstant (const char * name)
{
	struct ConstantDef * con = UT_Alloc (sizeof(struct ConstantDef));

	con->name = name;

	return con;
}

struct GlobalVar * NewGlobal (const char * name)
{
	struct GlobalVar * glob = UT_Alloc (sizeof(struct GlobalVar));

	if (num_global_vars >= 240)
		Fatal_Error ("too many global variables (> 240)\n");

	glob->id   = 0x10 + num_global_vars;
	glob->name = name;

	num_global_vars += 1;

	return glob;
}

struct LabelDef * NewLabel (const char * name)
{
	struct LabelDef * lab = UT_Alloc (sizeof(struct LabelDef));

	lab->name = name;
	lab->addr = -1;

	return lab;
}

struct LocalVar * NewLocalVar (const char * name, int offset)
{
	struct LocalVar * var = UT_Alloc (sizeof(struct LocalVar));

	var->name   = name;
	var->offset = offset;

	return var;
}

struct FunctionDef * NewFunction (void)
{
	struct FunctionDef * fun = UT_Alloc (sizeof(struct FunctionDef));

	return fun;
}

//----------------------------------------------------------------------

struct LabelDef * F_FindPrivateLabel (struct FunctionDef * fun, const char * name)
{
	struct LabelDef * lab;
	for (lab = fun->labels ; lab != NULL ; lab = lab->next)
	{
		if (strcmp (lab->name, name) == 0)
			return lab;
	}

	// not found
	return NULL;
}

struct LocalVar * F_FindLocalVar (struct FunctionDef * fun, const char * name)
{
	struct LocalVar * var;
	for (var = fun->locals ; var != NULL ; var = var->next)
	{
		if (strcmp (var->name, name) == 0)
			return var;
	}

	// not found
	return NULL;
}

struct LabelDef * F_AddPrivateLabel (struct FunctionDef * fun, const char * name)
{
	struct LabelDef * lab = NewLabel (name);

	// add to head of list
	lab->next   = fun->labels;
	fun->labels = lab;

	return lab;
}

struct LocalVar * F_AddLocalVar (struct FunctionDef * fun, const char * name)
{
	struct LocalVar * var = NewLocalVar (name, fun->num_locals * 4);
	fun->num_locals += 1;

	// add to head of list
	var->next   = fun->locals;
	fun->locals = var;

	return var;
}

//----------------------------------------------------------------------

struct ConstantDef * DefineConstant (const char * name, struct Node * expr)
{
	// caller must check that the symbol does not exist already

	struct Definition * def = SYM_Add (all_symbols, DEF_Constant, name);

	def->u.con = NewConstant (name);
	def->u.con->raw = expr;

	return def->u.con;
}

struct GlobalVar * DefineGlobal (const char * name, struct Node * expr)
{
	// caller must check that the symbol does not exist already

	struct Definition * def = SYM_Add (all_symbols, DEF_Global, name);

	def->u.glob = NewGlobal (name);
	def->u.glob->raw = expr;

	return def->u.glob;
}

struct LabelDef * DefineLabel (const char * name)
{
	// caller must check that the symbol does not exist already

	struct Definition * def = SYM_Add (all_symbols, DEF_Label, name);

	def->u.lab = NewLabel (name);

	return def->u.lab;
}

//----------------------------------------------------------------------

const char *const reserved_keywords[] =
{
	// special words
	"drop", "pop", "push", "pull", "hilo",
	"rtrue", "rfalse", "fail",

	// built-in constants
	"FALSE", "TRUE", "NULL",
	"NAN", "INFINITY", "STACKREF",

	// end of list
	NULL
};

bool ValidateName (const char * name, const char * what, bool is_local)
{
	if (name[0] == '.')
	{
		PostError ("%s names cannot begin with '.'", what);
		return P_FAIL;
	}

	// local variables can shadow global identifiers
	if (! is_local && SYM_Find (all_symbols, name) != NULL)
	{
		PostError ("symbol '%s' is already defined", name);
		return false;
	}

	int i;
	for (i = 0 ; reserved_keywords[i] != NULL ; i++)
	{
		if (strcmp (reserved_keywords[i], name) == 0)
		{
			PostError ("bad %s name ('%s' is a special keyword)", what, name);
			return false;
		}
	}

	// okay!
	return true;
}

//----------------------------------------------------------------------

bool EvalConstantPass (void)
{
	// returns true when all constants are evaluated.

	struct Definition * def;
	int num_left = 0;

	SYM_Begin (all_symbols);

	while (NULL != (def = SYM_Next (all_symbols)))
	{
		if (def->kind != DEF_Constant)
			continue;

		struct ConstantDef * con = def->u.con;

		if (con->value != NULL)
			continue;

		num_left += 1;

		struct Node * V = Evaluate (con->raw, ECTX_Constant, NULL);

		// unable to evaluate yet?
		if (V == P_UNKNOWN)
			continue;

		// NOTE: V may be P_FAIL, that is okay
		con->value = V;
	}

	return (num_left == 0);
}

void EvaluateConstants (void)
{
	// limit number of loops, in case of cyclic dependencies
	int loop;
	for (loop = 0 ; loop < 50 ; loop++)
	{
		if (EvalConstantPass ())
			return;
	}

	// WISH: give an example
	Fatal_Error ("cyclic dependencies with constants\n");
}
