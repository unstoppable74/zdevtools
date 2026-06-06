// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* symbol tables */

enum DefKind
{
	DEF_Constant = 1,
	DEF_Label    = 2,
	DEF_Global   = 3,  // Z-code only
};

struct Definition
{
	enum DefKind  kind;
	const char *  name;

	union
	{
		struct ConstantDef * con;
		struct LabelDef    * lab;
		struct GlobalVar   * glob;
	} u;

	struct Definition * next;
};

struct SymbolTable;

struct SymbolTable * SYM_New    (void);
void                 SYM_Free   (struct SymbolTable * tab);

struct Definition  * SYM_Find   (struct SymbolTable * tab, const char * name);
struct Definition  * SYM_Add    (struct SymbolTable * tab, enum DefKind kind, const char * name);
void                 SYM_Remove (struct SymbolTable * tab, const char * name);

void                 SYM_Begin  (struct SymbolTable * tab);
struct Definition  * SYM_Next   (struct SymbolTable * tab);
void                 SYM_Dump   (struct SymbolTable * tab);
