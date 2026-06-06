// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* instruction opcodes */

enum InstructionOp
{
	OP_NOP             = 0x00,
	OP_ADD             = 0x10,
	OP_SUB             = 0x11,
	OP_MUL             = 0x12,
	OP_DIV             = 0x13,
	OP_MOD             = 0x14,
	OP_NEG             = 0x15,
	OP_BITAND          = 0x18,
	OP_BITOR           = 0x19,
	OP_BITXOR          = 0x1A,
	OP_BITNOT          = 0x1B,
	OP_SHIFTL          = 0x1C,
	OP_SSHIFTR         = 0x1D,
	OP_USHIFTR         = 0x1E,
	OP_JUMP            = 0x20,
	OP_JZ              = 0x22,
	OP_JNZ             = 0x23,
	OP_JEQ             = 0x24,
	OP_JNE             = 0x25,
	OP_JLT             = 0x26,
	OP_JGE             = 0x27,
	OP_JGT             = 0x28,
	OP_JLE             = 0x29,
	OP_JLTU            = 0x2A,
	OP_JGEU            = 0x2B,
	OP_JGTU            = 0x2C,
	OP_JLEU            = 0x2D,
	OP_CALL            = 0x30,
	OP_RETURN          = 0x31,
	OP_CATCH           = 0x32,
	OP_THROW           = 0x33,
	OP_TAILCALL        = 0x34,
	OP_COPY            = 0x40,
	OP_COPYS           = 0x41,
	OP_COPYB           = 0x42,
	OP_SEXS            = 0x44,
	OP_SEXB            = 0x45,
	OP_ALOAD           = 0x48,
	OP_ALOADS          = 0x49,
	OP_ALOADB          = 0x4A,
	OP_ALOADBIT        = 0x4B,
	OP_ASTORE          = 0x4C,
	OP_ASTORES         = 0x4D,
	OP_ASTOREB         = 0x4E,
	OP_ASTOREBIT       = 0x4F,
	OP_STKCOUNT        = 0x50,
	OP_STKPEEK         = 0x51,
	OP_STKSWAP         = 0x52,
	OP_STKROLL         = 0x53,
	OP_STKCOPY         = 0x54,
	OP_STREAMCHAR      = 0x70,
	OP_STREAMNUM       = 0x71,
	OP_STREAMSTR       = 0x72,
	OP_STREAMUNICHAR   = 0x73,

	OP_GESTALT         = 0x100,
	OP_DEBUGTRAP       = 0x101,
	OP_GETMEMSIZE      = 0x102,
	OP_SETMEMSIZE      = 0x103,
	OP_JUMPABS         = 0x104,
	OP_RANDOM          = 0x110,
	OP_SETRANDOM       = 0x111,
	OP_QUIT            = 0x120,
	OP_VERIFY          = 0x121,
	OP_RESTART         = 0x122,
	OP_SAVE            = 0x123,
	OP_RESTORE         = 0x124,
	OP_SAVEUNDO        = 0x125,
	OP_RESTOREUNDO     = 0x126,
	OP_PROTECT         = 0x127,
	OP_HASUNDO         = 0x128,
	OP_DISCARDUNDO     = 0x129,
	OP_GLK             = 0x130,
	OP_GETSTRINGTBL    = 0x140,
	OP_SETSTRINGTBL    = 0x141,
	OP_GETIOSYS        = 0x148,
	OP_SETIOSYS        = 0x149,
	OP_LINEARSEARCH    = 0x150,
	OP_BINARYSEARCH    = 0x151,
	OP_LINKEDSEARCH    = 0x152,
	OP_CALLF           = 0x160,
	OP_CALLFI          = 0x161,
	OP_CALLFII         = 0x162,
	OP_CALLFIII        = 0x163,
	OP_MZERO           = 0x170,
	OP_MCOPY           = 0x171,
	OP_MALLOC          = 0x178,
	OP_MFREE           = 0x179,
	OP_ACCELFUNC       = 0x180,
	OP_ACCELPARAM      = 0x181,

	OP_NUMTOF          = 0x190,
	OP_FTONUMZ         = 0x191,
	OP_FTONUMN         = 0x192,
	OP_CEIL            = 0x198,
	OP_FLOOR           = 0x199,
	OP_FADD            = 0x1A0,
	OP_FSUB            = 0x1A1,
	OP_FMUL            = 0x1A2,
	OP_FDIV            = 0x1A3,
	OP_FMOD            = 0x1A4,
	OP_SQRT            = 0x1A8,
	OP_EXP             = 0x1A9,
	OP_LOG             = 0x1AA,
	OP_POW             = 0x1AB,
	OP_SIN             = 0x1B0,
	OP_COS             = 0x1B1,
	OP_TAN             = 0x1B2,
	OP_ASIN            = 0x1B3,
	OP_ACOS            = 0x1B4,
	OP_ATAN            = 0x1B5,
	OP_ATAN2           = 0x1B6,
	OP_JFEQ            = 0x1C0,
	OP_JFNE            = 0x1C1,
	OP_JFLT            = 0x1C2,
	OP_JFLE            = 0x1C3,
	OP_JFGT            = 0x1C4,
	OP_JFGE            = 0x1C5,
	OP_JISNAN          = 0x1C8,
	OP_JISINF          = 0x1C9,

	OP_NUMTOD          = 0x200,
	OP_DTONUMZ         = 0x201,
	OP_DTONUMN         = 0x202,
	OP_FTOD            = 0x203,
	OP_DTOF            = 0x204,
	OP_DCEIL           = 0x208,
	OP_DFLOOR          = 0x209,
	OP_DADD            = 0x210,
	OP_DSUB            = 0x211,
	OP_DMUL            = 0x212,
	OP_DDIV            = 0x213,
	OP_DMODR           = 0x214,
	OP_DMODQ           = 0x215,
	OP_DSQRT           = 0x218,
	OP_DEXP            = 0x219,
	OP_DLOG            = 0x21A,
	OP_DPOW            = 0x21B,
	OP_DSIN            = 0x220,
	OP_DCOS            = 0x221,
	OP_DTAN            = 0x222,
	OP_DASIN           = 0x223,
	OP_DACOS           = 0x224,
	OP_DATAN           = 0x225,
	OP_DATAN2          = 0x226,
	OP_JDEQ            = 0x230,
	OP_JDNE            = 0x231,
	OP_JDLT            = 0x232,
	OP_JDLE            = 0x233,
	OP_JDGT            = 0x234,
	OP_JDGE            = 0x235,
	OP_JDISNAN         = 0x238,
	OP_JDISINF         = 0x239,

	/* these are NOT real opcodes */

	OP_FAKE_PUSH       = 0xF001,
	OP_FAKE_PULL       = 0xF002,

	OP_FAKE_DCOPY      = 0xF003,
	OP_FAKE_DLOAD      = 0xF004,
	OP_FAKE_DSTORE     = 0xF005,
	OP_FAKE_DPUSH      = 0xF006,
	OP_FAKE_DPULL      = 0xF007,
};

enum AddressingMode
{
	/* load only */

	ADR_Const_Zero     = 0,
	ADR_Const_Byte     = 1,
	ADR_Const_Word     = 2,
	ADR_Const_Long     = 3,

	ADR_Pop            = 8,

	/* save only */

	ADR_Drop           = 0,
	ADR_Push           = 8,

	/* common */

	ADR_Address_Byte   = 5,
	ADR_Address_Word   = 6,
	ADR_Address_Long   = 7,

	ADR_Local_Byte     = 9,
	ADR_Local_Word     = 10,
	ADR_Local_Long     = 11,

	ADR_RAM_Byte       = 13,
	ADR_RAM_Word       = 14,
	ADR_RAM_Long       = 15,
};

enum ObjectType
{
	OBJ_FuncVA         = 0xC0,  // var-arg function
	OBJ_Function       = 0xC1,  // normal  function

	OBJ_LatinString    = 0xE0,  // one byte per character
	OBJ_HuffString     = 0xE1,  // huffman encoded
	OBJ_UnicodeString  = 0xE2,  // one long per character
};

enum OpcodeInfoFlags
{
	OPFLAG_JUMP    = (1 << 0),  // a *relative* jump
	OPFLAG_FLOAT   = (1 << 1),  // 32-bit floating point
	OPFLAG_DOUBLE  = (1 << 2),  // 64-bit floating point

	// Z-code only flags:
	OPFLAG_ZSTORE  = (1 << 3),  // does a store
	OPFLAG_ZVAR    = (1 << 4),  // operand refers to a var
	OPFLAG_ZCALL   = (1 << 5),  // operand refers to a routine
	OPFLAG_ZTEXT   = (1 << 6),  // operand must be a literal string
	OPFLAG_ZNEG    = (1 << 7),  // jump has negated sense
};

struct OpcodeInfo
{
	const char * name;
	enum InstructionOp  op;
	int  loads;
	int  stores;
	int  flags;
};

const struct OpcodeInfo * LookupOp (const char * name);
