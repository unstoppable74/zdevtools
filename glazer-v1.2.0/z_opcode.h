// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* Z-machine opcodes */

// a few version-specific codes are or'd with 0x100 or 0x200.
// extended opcodes are or'd with 0x400.

enum Z_InstructionOp
{
	/* long form */

	ZOP_JE               = 0x01,
	ZOP_JL               = 0x02,
	ZOP_JG               = 0x03,
	ZOP_DEC_CHK          = 0x04,
	ZOP_INC_CHK          = 0x05,
	ZOP_JIN              = 0x06,
	ZOP_TEST             = 0x07,
	ZOP_OR               = 0x08,
	ZOP_AND              = 0x09,
	ZOP_TEST_ATTR        = 0x0A,
	ZOP_SET_ATTR         = 0x0B,
	ZOP_CLEAR_ATTR       = 0x0C,
	ZOP_STORE            = 0x0D,
	ZOP_INSERT_OBJ       = 0x0E,
	ZOP_LOADW            = 0x0F,

	ZOP_LOADB            = 0x10,
	ZOP_GET_PROP         = 0x11,
	ZOP_GET_PROP_ADDR    = 0x12,
	ZOP_GET_NEXT_PROP    = 0x13,
	ZOP_ADD              = 0x14,
	ZOP_SUB              = 0x15,
	ZOP_MUL              = 0x16,
	ZOP_DIV              = 0x17,
	ZOP_MOD              = 0x18,
	ZOP_CALL_2S          = 0x19,
	ZOP_CALL_2N          = 0x1A,
	ZOP_SET_COLOUR       = 0x1B,
	ZOP_THROW            = 0x1C,

	/* short form */

	ZOP_JZ               = 0x80,
	ZOP_GET_SIBLING      = 0x81,
	ZOP_GET_CHILD        = 0x82,
	ZOP_GET_PARENT       = 0x83,
	ZOP_GET_PROP_LEN     = 0x84,
	ZOP_INC              = 0x85,
	ZOP_DEC              = 0x86,
	ZOP_PRINT_ADDR       = 0x87,
	ZOP_CALL_1S          = 0x88,
	ZOP_REMOVE_OBJ       = 0x89,
	ZOP_PRINT_OBJ        = 0x8A,
	ZOP_RET              = 0x8B,
	ZOP_JUMP             = 0x8C,
	ZOP_PRINT_PADDR      = 0x8D,
	ZOP_LOAD             = 0x8E,
	ZOP_CALL_1N          = 0x8F,
	ZOP_NOT_Z1           = 0x8F | 0x100,

	/* empty form */

	ZOP_RTRUE            = 0xB0,
	ZOP_RFALSE           = 0xB1,
	ZOP_PRINT            = 0xB2,
	ZOP_PRINT_RET        = 0xB3,
	ZOP_NOP              = 0xB4,
	ZOP_SAVE_Z1          = 0xB5 | 0x100,
	ZOP_SAVE_Z4          = 0xB5,
	ZOP_RESTORE_Z1       = 0xB6 | 0x100,
	ZOP_RESTORE_Z4       = 0xB6,
	ZOP_RESTART          = 0xB7,
	ZOP_RET_POPPED       = 0xB8,
	ZOP_CATCH            = 0xB9,
	ZOP_POP_Z1           = 0xB9 | 0x100,
	ZOP_QUIT             = 0xBA,
	ZOP_NEW_LINE         = 0xBB,
	ZOP_SHOW_STATUS      = 0xBC,
	ZOP_VERIFY           = 0xBD,
	ZOP_PIRACY           = 0xBF,

	/* multi form */

	ZOP_CALL_VS          = 0xE0,
	ZOP_CALL_Z1          = 0xE0 | 0x100,
	ZOP_STOREW           = 0xE1,
	ZOP_STOREB           = 0xE2,
	ZOP_PUT_PROP         = 0xE3,
	ZOP_READ             = 0xE4,
	ZOP_READ_Z1          = 0xE4 | 0x100,
	ZOP_READ_Z4          = 0xE4 | 0x200,
	ZOP_PRINT_CHAR       = 0xE5,
	ZOP_PRINT_NUM        = 0xE6,
	ZOP_RANDOM           = 0xE7,
	ZOP_PUSH             = 0xE8,
	ZOP_PULL             = 0xE9,
	ZOP_PULL_Z6          = 0xE9 | 0x200,
	ZOP_SPLIT_WINDOW     = 0xEA,
	ZOP_SET_WINDOW       = 0xEB,
	ZOP_CALL_VS2         = 0xEC,
	ZOP_ERASE_WINDOW     = 0xED,
	ZOP_ERASE_LINE       = 0xEE,
	ZOP_SET_CURSOR       = 0xEF,

	ZOP_GET_CURSOR       = 0xF0,
	ZOP_SET_TEXT_STYLE   = 0xF1,
	ZOP_BUFFER_MODE      = 0xF2,
	ZOP_OUTPUT_STREAM    = 0xF3,
	ZOP_INPUT_STREAM     = 0xF4,
	ZOP_SOUND_EFFECT     = 0xF5,
	ZOP_READ_CHAR        = 0xF6,
	ZOP_SCAN_TABLE       = 0xF7,
	ZOP_NOT              = 0xF8,
	ZOP_CALL_VN          = 0xF9,
	ZOP_CALL_VN2         = 0xFA,
	ZOP_TOKENISE         = 0xFB,
	ZOP_ENCODE_TEXT      = 0xFC,
	ZOP_COPY_TABLE       = 0xFD,
	ZOP_PRINT_TABLE      = 0xFE,
	ZOP_CHECK_ARG_COUNT  = 0xFF,

	/* extended form */

	ZOP_SAVE             = 0x00 | 0x400,
	ZOP_RESTORE          = 0x01 | 0x400,
	ZOP_LOG_SHIFT        = 0x02 | 0x400,
	ZOP_ART_SHIFT        = 0x03 | 0x400,
	ZOP_SET_FONT         = 0x04 | 0x400,
	ZOP_DRAW_PICTURE     = 0x05 | 0x400,
	ZOP_PICTURE_DATA     = 0x06 | 0x400,
	ZOP_ERASE_PICTURE    = 0x07 | 0x400,
	ZOP_SET_MARGINS      = 0x08 | 0x400,
	ZOP_SAVE_UNDO        = 0x09 | 0x400,
	ZOP_RESTORE_UNDO     = 0x0A | 0x400,
	ZOP_PRINT_UNICODE    = 0x0B | 0x400,
	ZOP_CHECK_UNICODE    = 0x0C | 0x400,
	ZOP_SET_TRUE_COLOUR  = 0x0D | 0x400,

	ZOP_MOVE_WINDOW      = 0x10 | 0x400,
	ZOP_WINDOW_SIZE      = 0x11 | 0x400,
	ZOP_WINDOW_STYLE     = 0x12 | 0x400,
	ZOP_GET_WIND_PROP    = 0x13 | 0x400,
	ZOP_SCROLL_WINDOW    = 0x14 | 0x400,
	ZOP_POP_STACK        = 0x15 | 0x400,
	ZOP_READ_MOUSE       = 0x16 | 0x400,
	ZOP_MOUSE_WINDOW     = 0x17 | 0x400,
	ZOP_PUSH_STACK       = 0x18 | 0x400,
	ZOP_PUT_WIND_PROP    = 0x19 | 0x400,
	ZOP_PRINT_FORM       = 0x1A | 0x400,
	ZOP_MAKE_MENU        = 0x1B | 0x400,
	ZOP_PICTURE_TABLE    = 0x1C | 0x400,
	ZOP_BUFFER_SCREEN    = 0x1D | 0x400,
};

enum Z_AddressingMode
{
	ZADR_Large    = 0,   // 16-bit constant
	ZADR_Small    = 1,   //  8-bit constant
	ZADR_Variable = 2,   //  1 byte, local or global var or stack
	ZADR_None     = 3,   //  nothing at all
};

enum Z_InstructionForm
{
	ZFORM_Long     = 0,
	ZFORM_Short    = 1,
	ZFORM_Empty    = 2,  // short, but no operands
	ZFORM_Multi    = 3,
	ZFORM_Extended = 4,
};

struct Z_OpcodeInfo
{
	const char * name;
	enum Z_InstructionOp  op;
	int  version_low;
	int  version_high;
	int  arg_min;
	int  arg_max;
	int  flags;
};

extern int z_version;
extern int z_scale_factor;

const struct Z_OpcodeInfo * Z_LookupOp (const char * name, bool ignore_version);

enum Z_InstructionForm Z_CalcForm (const struct Z_OpcodeInfo * info);
