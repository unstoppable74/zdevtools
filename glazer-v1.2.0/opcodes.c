// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

// Note that for OPFLAG_JUMP, there is an implicit operand which
// follows any others.  for `catch`, it follows the store!

static const struct OpcodeInfo  all_opcodes[] =
{
	{ "nop",              OP_NOP,              0, 0, 0 },
	{ "add",              OP_ADD,              2, 1, 0 },
	{ "sub",              OP_SUB,              2, 1, 0 },
	{ "mul",              OP_MUL,              2, 1, 0 },
	{ "div",              OP_DIV,              2, 1, 0 },
	{ "mod",              OP_MOD,              2, 1, 0 },
	{ "neg",              OP_NEG,              1, 1, 0 },
	{ "bitand",           OP_BITAND,           2, 1, 0 },
	{ "bitor",            OP_BITOR,            2, 1, 0 },
	{ "bitxor",           OP_BITXOR,           2, 1, 0 },
	{ "bitnot",           OP_BITNOT,           1, 1, 0 },
	{ "shiftl",           OP_SHIFTL,           2, 1, 0 },
	{ "sshiftr",          OP_SSHIFTR,          2, 1, 0 },
	{ "ushiftr",          OP_USHIFTR,          2, 1, 0 },

	{ "jump",             OP_JUMP,             0, 0, OPFLAG_JUMP },
	{ "jz",               OP_JZ,               1, 0, OPFLAG_JUMP },
	{ "jnz",              OP_JNZ,              1, 0, OPFLAG_JUMP },
	{ "jeq",              OP_JEQ,              2, 0, OPFLAG_JUMP },
	{ "jne",              OP_JNE,              2, 0, OPFLAG_JUMP },
	{ "jlt",              OP_JLT,              2, 0, OPFLAG_JUMP },
	{ "jge",              OP_JGE,              2, 0, OPFLAG_JUMP },
	{ "jgt",              OP_JGT,              2, 0, OPFLAG_JUMP },
	{ "jle",              OP_JLE,              2, 0, OPFLAG_JUMP },
	{ "jltu",             OP_JLTU,             2, 0, OPFLAG_JUMP },
	{ "jgeu",             OP_JGEU,             2, 0, OPFLAG_JUMP },
	{ "jgtu",             OP_JGTU,             2, 0, OPFLAG_JUMP },
	{ "jleu",             OP_JLEU,             2, 0, OPFLAG_JUMP },

	{ "call",             OP_CALL,             2, 1, 0 },
	{ "return",           OP_RETURN,           1, 0, 0 },
	{ "catch",            OP_CATCH,            0, 1, OPFLAG_JUMP },
	{ "throw",            OP_THROW,            2, 0, 0 },
	{ "tailcall",         OP_TAILCALL,         2, 0, 0 },
	{ "copy",             OP_COPY,             1, 1, 0 },
	{ "copys",            OP_COPYS,            1, 1, 0 },
	{ "copyb",            OP_COPYB,            1, 1, 0 },
	{ "sexs",             OP_SEXS,             1, 1, 0 },
	{ "sexb",             OP_SEXB,             1, 1, 0 },
	{ "aload",            OP_ALOAD,            2, 1, 0 },
	{ "aloads",           OP_ALOADS,           2, 1, 0 },
	{ "aloadb",           OP_ALOADB,           2, 1, 0 },
	{ "aloadbit",         OP_ALOADBIT,         2, 1, 0 },
	{ "astore",           OP_ASTORE,           3, 0, 0 },
	{ "astores",          OP_ASTORES,          3, 0, 0 },
	{ "astoreb",          OP_ASTOREB,          3, 0, 0 },
	{ "astorebit",        OP_ASTOREBIT,        3, 0, 0 },
	{ "stkcount",         OP_STKCOUNT,         0, 1, 0 },
	{ "stkpeek",          OP_STKPEEK,          1, 1, 0 },
	{ "stkswap",          OP_STKSWAP,          0, 0, 0 },
	{ "stkroll",          OP_STKROLL,          2, 0, 0 },
	{ "stkcopy",          OP_STKCOPY,          1, 0, 0 },
	{ "streamchar",       OP_STREAMCHAR,       1, 0, 0 },
	{ "streamnum",        OP_STREAMNUM,        1, 0, 0 },
	{ "streamstr",        OP_STREAMSTR,        1, 0, 0 },
	{ "streamunichar",    OP_STREAMUNICHAR,    1, 0, 0 },

	{ "gestalt",          OP_GESTALT,          2, 1, 0 },
	{ "debugtrap",        OP_DEBUGTRAP,        1, 0, 0 },
	{ "getmemsize",       OP_GETMEMSIZE,       0, 1, 0 },
	{ "setmemsize",       OP_SETMEMSIZE,       1, 1, 0 },
	{ "jumpabs",          OP_JUMPABS,          1, 0, 0 },
	{ "random",           OP_RANDOM,           1, 1, 0 },
	{ "setrandom",        OP_SETRANDOM,        1, 0, 0 },
	{ "quit",             OP_QUIT,             0, 0, 0 },
	{ "verify",           OP_VERIFY,           0, 1, 0 },
	{ "restart",          OP_RESTART,          0, 0, 0 },
	{ "save",             OP_SAVE,             1, 1, 0 },
	{ "restore",          OP_RESTORE,          1, 1, 0 },
	{ "saveundo",         OP_SAVEUNDO,         0, 1, 0 },
	{ "restoreundo",      OP_RESTOREUNDO,      0, 1, 0 },
	{ "protect",          OP_PROTECT,          2, 0, 0 },
	{ "hasundo",          OP_HASUNDO,          0, 1, 0 },
	{ "discardundo",      OP_DISCARDUNDO,      0, 0, 0 },

	{ "glk",              OP_GLK,              2, 1, 0 },
	{ "getstringtbl",     OP_GETSTRINGTBL,     0, 1, 0 },
	{ "setstringtbl",     OP_SETSTRINGTBL,     1, 0, 0 },
	{ "getiosys",         OP_GETIOSYS,         0, 2, 0 },
	{ "setiosys",         OP_SETIOSYS,         2, 0, 0 },
	{ "linearsearch",     OP_LINEARSEARCH,     7, 1, 0 },
	{ "binarysearch",     OP_BINARYSEARCH,     7, 1, 0 },
	{ "linkedsearch",     OP_LINKEDSEARCH,     6, 1, 0 },
	{ "callf",            OP_CALLF,            1, 1, 0 },
	{ "callfi",           OP_CALLFI,           2, 1, 0 },
	{ "callfii",          OP_CALLFII,          3, 1, 0 },
	{ "callfiii",         OP_CALLFIII,         4, 1, 0 },
	{ "mzero",            OP_MZERO,            2, 0, 0 },
	{ "mcopy",            OP_MCOPY,            3, 0, 0 },
	{ "malloc",           OP_MALLOC,           1, 1, 0 },
	{ "mfree",            OP_MFREE,            1, 0, 0 },
	{ "accelfunc",        OP_ACCELFUNC,        2, 0, 0 },
	{ "accelparam",       OP_ACCELPARAM,       2, 0, 0 },

	{ "numtof",           OP_NUMTOF,           1, 1, OPFLAG_FLOAT },
	{ "ftonumz",          OP_FTONUMZ,          1, 1, OPFLAG_FLOAT },
	{ "ftonumn",          OP_FTONUMN,          1, 1, OPFLAG_FLOAT },
	{ "ceil",             OP_CEIL,             1, 1, OPFLAG_FLOAT },
	{ "floor",            OP_FLOOR,            1, 1, OPFLAG_FLOAT },
	{ "fadd",             OP_FADD,             2, 1, OPFLAG_FLOAT },
	{ "fsub",             OP_FSUB,             2, 1, OPFLAG_FLOAT },
	{ "fmul",             OP_FMUL,             2, 1, OPFLAG_FLOAT },
	{ "fdiv",             OP_FDIV,             2, 1, OPFLAG_FLOAT },
	{ "fmod",             OP_FMOD,             2, 2, OPFLAG_FLOAT },
	{ "sqrt",             OP_SQRT,             1, 1, OPFLAG_FLOAT },
	{ "exp",              OP_EXP,              1, 1, OPFLAG_FLOAT },
	{ "log",              OP_LOG,              1, 1, OPFLAG_FLOAT },
	{ "pow",              OP_POW,              2, 1, OPFLAG_FLOAT },
	{ "sin",              OP_SIN,              1, 1, OPFLAG_FLOAT },
	{ "cos",              OP_COS,              1, 1, OPFLAG_FLOAT },
	{ "tan",              OP_TAN,              1, 1, OPFLAG_FLOAT },
	{ "asin",             OP_ASIN,             1, 1, OPFLAG_FLOAT },
	{ "acos",             OP_ACOS,             1, 1, OPFLAG_FLOAT },
	{ "atan",             OP_ATAN,             1, 1, OPFLAG_FLOAT },
	{ "atan2",            OP_ATAN2,            2, 1, OPFLAG_FLOAT },
	{ "jfeq",             OP_JFEQ,             3, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jfne",             OP_JFNE,             3, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jflt",             OP_JFLT,             2, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jfle",             OP_JFLE,             2, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jfgt",             OP_JFGT,             2, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jfge",             OP_JFGE,             2, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jisnan",           OP_JISNAN,           1, 0, OPFLAG_JUMP | OPFLAG_FLOAT },
	{ "jisinf",           OP_JISINF,           1, 0, OPFLAG_JUMP | OPFLAG_FLOAT },

	{ "numtod",           OP_NUMTOD,           1, 2, OPFLAG_DOUBLE },
	{ "dtonumz",          OP_DTONUMZ,          2, 1, OPFLAG_DOUBLE },
	{ "dtonumn",          OP_DTONUMN,          2, 1, OPFLAG_DOUBLE },
	{ "ftod",             OP_FTOD,             1, 2, OPFLAG_DOUBLE },
	{ "dtof",             OP_DTOF,             2, 1, OPFLAG_DOUBLE },
	{ "dceil",            OP_DCEIL,            2, 2, OPFLAG_DOUBLE },
	{ "dfloor",           OP_DFLOOR,           2, 2, OPFLAG_DOUBLE },
	{ "dadd",             OP_DADD,             4, 2, OPFLAG_DOUBLE },
	{ "dsub",             OP_DSUB,             4, 2, OPFLAG_DOUBLE },
	{ "dmul",             OP_DMUL,             4, 2, OPFLAG_DOUBLE },
	{ "ddiv",             OP_DDIV,             4, 2, OPFLAG_DOUBLE },
	{ "dmodr",            OP_DMODR,            4, 2, OPFLAG_DOUBLE },
	{ "dmodq",            OP_DMODQ,            4, 2, OPFLAG_DOUBLE },
	{ "dsqrt",            OP_DSQRT,            2, 2, OPFLAG_DOUBLE },
	{ "dexp",             OP_DEXP,             2, 2, OPFLAG_DOUBLE },
	{ "dlog",             OP_DLOG,             2, 2, OPFLAG_DOUBLE },
	{ "dpow",             OP_DPOW,             4, 2, OPFLAG_DOUBLE },
	{ "dsin",             OP_DSIN,             2, 2, OPFLAG_DOUBLE },
	{ "dcos",             OP_DCOS,             2, 2, OPFLAG_DOUBLE },
	{ "dtan",             OP_DTAN,             2, 2, OPFLAG_DOUBLE },
	{ "dasin",            OP_DASIN,            2, 2, OPFLAG_DOUBLE },
	{ "dacos",            OP_DACOS,            2, 2, OPFLAG_DOUBLE },
	{ "datan",            OP_DATAN,            2, 2, OPFLAG_DOUBLE },
	{ "datan2",           OP_DATAN2,           4, 2, OPFLAG_DOUBLE },
	{ "jdeq",             OP_JDEQ,             6, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdne",             OP_JDNE,             6, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdlt",             OP_JDLT,             4, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdle",             OP_JDLE,             4, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdgt",             OP_JDGT,             4, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdge",             OP_JDGE,             4, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdisnan",          OP_JDISNAN,          2, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },
	{ "jdisinf",          OP_JDISINF,          2, 0, OPFLAG_JUMP | OPFLAG_DOUBLE },

	// these are NOT real opcodes
	{ "push",             OP_FAKE_PUSH,        1, 0, 0 },
	{ "pull",             OP_FAKE_PULL,        0, 1, 0 },

	{ "dcopy",            OP_FAKE_DCOPY,       2, 2, 0 },
	{ "dload",            OP_FAKE_DLOAD,       1, 2, 0 },
	{ "dstore",           OP_FAKE_DSTORE,      3, 0, 0 },
	{ "dpush",            OP_FAKE_DPUSH,       2, 0, 0 },
	{ "dpull",            OP_FAKE_DPULL,       0, 2, 0 },

	// end of list
	{ NULL, OP_NOP, 0, 0, 0 }
};

const struct OpcodeInfo * LookupOp (const char * name)
{
	// WISH: might be faster to use a symbol table to find opcodes,
	//       however it is very possible to not provide much gain,
	//       so benchmark it first on a large real-world example.

	int i;
	for (i = 0 ; all_opcodes[i].name != NULL ; i++)
	{
		const struct OpcodeInfo * info = &all_opcodes[i];

		if (strcmp (info->name, name) == 0)
			return info;
	}

	// not found
	return NULL;
}
