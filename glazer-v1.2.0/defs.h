// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* constant, label and function definitions */

struct ConstantDef
{
	const char * name;

	// the unevaluated expression
	struct Node * raw;

	// the final value, NULL when not evaluated yet, or P_FAIL
	// when the expression could not be evaluated.
	struct Node * value;
};

struct GlobalVar
{
	const char * name;

	// the number used in instructions, 16..255
	int  id;

	// the unevaluated expression, NULL to use default (zero)
	struct Node * raw;
};

struct LabelDef
{
	const char * name;

	// the final address, -1 when not known yet
	int  addr;

	// which section the label belongs to
	enum SectionKind  section;

	// for a local label, link in parent function's list
	struct LabelDef * next;
};

struct LocalVar
{
	const char * name;

	int  offset;

	struct LocalVar * next;
};

struct FunctionDef
{
	// a list of local variables
	struct LocalVar * locals;
	int  num_locals;

	// a list of local labels
	struct LabelDef * labels;

	// which section the function belongs to
	enum SectionKind  section;
};


extern struct SymbolTable * all_symbols;

extern int num_global_vars;

void InitDefinitions (void);

struct ConstantDef * NewConstant (const char * name);
struct LabelDef    * NewLabel    (const char * name);
struct GlobalVar   * NewGlobal   (const char * name);
struct FunctionDef * NewFunction (void);
struct LocalVar    * NewLocalVar (const char * name, int offset);

struct LabelDef * F_FindPrivateLabel (struct FunctionDef * fun, const char * name);
struct LocalVar * F_FindLocalVar     (struct FunctionDef * fun, const char * name);

struct LabelDef * F_AddPrivateLabel (struct FunctionDef * fun, const char * name);
struct LocalVar * F_AddLocalVar     (struct FunctionDef * fun, const char * name);

struct ConstantDef * DefineConstant (const char * name, struct Node * expr);
struct GlobalVar   * DefineGlobal   (const char * name, struct Node * expr);
struct LabelDef    * DefineLabel    (const char * name);

bool ValidateName (const char * name, const char * what, bool is_local);

void EvaluateConstants (void);
