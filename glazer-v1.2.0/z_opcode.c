// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

// version of the Z-machine: z3 .. z8
int z_version;

// scaling factor: 2 / 4 / 8
int z_scale_factor;


static const struct Z_OpcodeInfo  z_opcodes[] =
{
	/* long form */

	{ "je",                ZOP_JE,                1,6, 2,2, OPFLAG_JUMP },
	{ "jne",               ZOP_JE,                1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZNEG },
	{ "jl",                ZOP_JL,                1,6, 2,2, OPFLAG_JUMP },
	{ "jge",               ZOP_JL,                1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZNEG },
	{ "jg",                ZOP_JG,                1,6, 2,2, OPFLAG_JUMP },
	{ "jle",               ZOP_JG,                1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZNEG},
	{ "dec_chk",           ZOP_DEC_CHK,           1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZVAR },
	{ "inc_chk",           ZOP_INC_CHK,           1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZVAR },
	{ "jin",               ZOP_JIN,               1,6, 2,2, OPFLAG_JUMP },
	{ "jnotin",            ZOP_JIN,               1,6, 2,2, OPFLAG_JUMP | OPFLAG_ZNEG },
	{ "test",              ZOP_TEST,              1,6, 2,2, OPFLAG_JUMP },
	{ "or",                ZOP_OR,                1,6, 2,2, OPFLAG_ZSTORE },
	{ "and",               ZOP_AND,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "test_attr",         ZOP_TEST_ATTR,         1,6, 2,2, OPFLAG_JUMP },
	{ "set_attr",          ZOP_SET_ATTR,          1,6, 2,2, 0 },
	{ "clear_attr",        ZOP_CLEAR_ATTR,        1,6, 2,2, 0 },
	{ "store",             ZOP_STORE,             1,6, 2,2, OPFLAG_ZVAR },
	{ "insert_obj",        ZOP_INSERT_OBJ,        1,6, 2,2, 0 },
	{ "loadw",             ZOP_LOADW,             1,6, 2,2, OPFLAG_ZSTORE },
	{ "loadb",             ZOP_LOADB,             1,6, 2,2, OPFLAG_ZSTORE },
	{ "get_prop",          ZOP_GET_PROP,          1,6, 2,2, OPFLAG_ZSTORE },
	{ "get_prop_addr",     ZOP_GET_PROP_ADDR,     1,6, 2,2, OPFLAG_ZSTORE },
	{ "get_next_prop",     ZOP_GET_NEXT_PROP,     1,6, 2,2, OPFLAG_ZSTORE },
	{ "add",               ZOP_ADD,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "sub",               ZOP_SUB,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "mul",               ZOP_MUL,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "div",               ZOP_DIV,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "mod",               ZOP_MOD,               1,6, 2,2, OPFLAG_ZSTORE },
	{ "call_2s",           ZOP_CALL_2S,           4,6, 2,2, OPFLAG_ZCALL | OPFLAG_ZSTORE },
	{ "call_2n",           ZOP_CALL_2N,           5,6, 2,2, OPFLAG_ZCALL },
	{ "set_colour",        ZOP_SET_COLOUR,        5,6, 2,2, 0 },
	{ "throw",             ZOP_THROW,             5,6, 2,2, 0 },

	/* short form */

	{ "jz",                ZOP_JZ,                1,6, 1,1, OPFLAG_JUMP },
	{ "jnz",               ZOP_JZ,                1,6, 1,1, OPFLAG_JUMP | OPFLAG_ZNEG },
	{ "get_sibling",       ZOP_GET_SIBLING,       1,6, 1,1, OPFLAG_ZSTORE | OPFLAG_JUMP },
	{ "get_child",         ZOP_GET_CHILD,         1,6, 1,1, OPFLAG_ZSTORE | OPFLAG_JUMP },
	{ "get_parent",        ZOP_GET_PARENT,        1,6, 1,1, OPFLAG_ZSTORE },
	{ "get_prop_len",      ZOP_GET_PROP_LEN,      1,6, 1,1, OPFLAG_ZSTORE },
	{ "inc",               ZOP_INC,               1,6, 1,1, OPFLAG_ZVAR },
	{ "dec",               ZOP_DEC,               1,6, 1,1, OPFLAG_ZVAR },
	{ "print_addr",        ZOP_PRINT_ADDR,        1,6, 1,1, 0 },
	{ "call_1s",           ZOP_CALL_1S,           4,6, 1,1, OPFLAG_ZCALL | OPFLAG_ZSTORE },
	{ "remove_obj",        ZOP_REMOVE_OBJ,        1,6, 1,1, 0 },
	{ "print_obj",         ZOP_PRINT_OBJ,         1,6, 1,1, 0 },
	{ "ret",               ZOP_RET,               1,6, 1,1, 0 },
	{ "return",            ZOP_RET,               1,6, 1,1, 0 },  /* synonym */
	{ "jump",              ZOP_JUMP,              1,6, 1,1, OPFLAG_JUMP },
	{ "print_paddr",       ZOP_PRINT_PADDR,       1,6, 1,1, 0 },
	{ "load",              ZOP_LOAD,              1,6, 1,1, OPFLAG_ZSTORE | OPFLAG_ZVAR },
	{ "not",               ZOP_NOT_Z1,            1,4, 1,1, OPFLAG_ZSTORE },
	{ "call_1n",           ZOP_CALL_1N,           5,6, 1,1, OPFLAG_ZCALL },

	/* empty form */

	{ "rtrue",             ZOP_RTRUE,             1,6, 0,0, 0 },
	{ "rfalse",            ZOP_RFALSE,            1,6, 0,0, 0 },
	{ "print",             ZOP_PRINT,             1,6, 1,1, OPFLAG_ZTEXT },
	{ "print_ret",         ZOP_PRINT_RET,         1,6, 1,1, OPFLAG_ZTEXT },
	{ "nop",               ZOP_NOP,               1,6, 0,0, 0 },
	{ "save",              ZOP_SAVE_Z1,           1,3, 0,0, OPFLAG_JUMP },
	{ "save",              ZOP_SAVE_Z4,           4,4, 0,0, OPFLAG_ZSTORE },
	{ "restore",           ZOP_RESTORE_Z1,        1,3, 0,0, OPFLAG_JUMP },
	{ "restore",           ZOP_RESTORE_Z4,        4,4, 0,0, OPFLAG_ZSTORE },
	{ "restart",           ZOP_RESTART,           1,6, 0,0, 0 },
	{ "ret_popped",        ZOP_RET_POPPED,        1,6, 0,0, 0 },
	{ "pop",               ZOP_POP_Z1,            1,4, 0,0, 0 },
	{ "catch",             ZOP_CATCH,             5,6, 0,0, OPFLAG_ZSTORE },
	{ "quit",              ZOP_QUIT,              1,6, 0,0, 0 },
	{ "new_line",          ZOP_NEW_LINE,          1,6, 0,0, 0 },
	{ "show_status",       ZOP_SHOW_STATUS,       3,3, 0,0, 0 },
	{ "verify",            ZOP_VERIFY,            3,6, 0,0, OPFLAG_JUMP },
	{ "piracy",            ZOP_PIRACY,            5,6, 0,0, OPFLAG_JUMP },

	/* multi form */

	{ "call",              ZOP_CALL_Z1,           1,3, 1,4, OPFLAG_ZCALL | OPFLAG_ZSTORE },
	{ "call_vs",           ZOP_CALL_VS,           4,6, 1,4, OPFLAG_ZCALL | OPFLAG_ZSTORE },
	{ "storew",            ZOP_STOREW,            1,6, 3,3, 0 },
	{ "storeb",            ZOP_STOREB,            1,6, 3,3, 0 },
	{ "put_prop",          ZOP_PUT_PROP,          1,6, 3,3, 0 },
	{ "read",              ZOP_READ_Z1,           1,3, 2,2, 0 },
	{ "read",              ZOP_READ_Z4,           4,4, 2,4, 0 },
	{ "read",              ZOP_READ,              5,6, 2,4, OPFLAG_ZSTORE },
	{ "print_char",        ZOP_PRINT_CHAR,        1,6, 1,1, 0 },
	{ "print_num",         ZOP_PRINT_NUM,         1,6, 1,1, 0 },
	{ "random",            ZOP_RANDOM,            1,6, 1,1, OPFLAG_ZSTORE },
	{ "push",              ZOP_PUSH,              1,6, 1,1, 0 },
	{ "pull",              ZOP_PULL,              1,5, 1,1, OPFLAG_ZVAR },
	{ "pull",              ZOP_PULL_Z6,           6,6, 1,2, OPFLAG_ZSTORE },
	{ "split_window",      ZOP_SPLIT_WINDOW,      3,6, 1,1, 0 },
	{ "set_window",        ZOP_SET_WINDOW,        3,6, 1,1, 0 },
	{ "call_vs2",          ZOP_CALL_VS2,          4,6, 1,8, OPFLAG_ZCALL | OPFLAG_ZSTORE },
	{ "erase_window",      ZOP_ERASE_WINDOW,      4,6, 1,1, 0 },
	{ "erase_line",        ZOP_ERASE_LINE,        4,6, 1,1, 0 },
	{ "set_cursor",        ZOP_SET_CURSOR,        4,6, 2,3, 0 },
	{ "get_cursor",        ZOP_GET_CURSOR,        4,6, 1,1, 0 },
	{ "set_text_style",    ZOP_SET_TEXT_STYLE,    4,6, 1,1, 0 },
	{ "buffer_mode",       ZOP_BUFFER_MODE,       4,6, 1,1, 0 },
	{ "output_stream",     ZOP_OUTPUT_STREAM,     3,6, 1,3, 0 },
	{ "input_stream",      ZOP_INPUT_STREAM,      3,6, 1,1, 0 },
	{ "sound_effect",      ZOP_SOUND_EFFECT,      3,6, 3,4, 0 },
	{ "read_char",         ZOP_READ_CHAR,         4,6, 1,3, OPFLAG_ZSTORE },
	{ "scan_table",        ZOP_SCAN_TABLE,        4,6, 3,4, OPFLAG_ZSTORE | OPFLAG_JUMP },
	{ "not",               ZOP_NOT,               5,6, 1,1, OPFLAG_ZSTORE },
	{ "call_vn",           ZOP_CALL_VN,           5,6, 1,4, OPFLAG_ZCALL },
	{ "call_vn2",          ZOP_CALL_VN2,          5,6, 1,8, OPFLAG_ZCALL },
	{ "tokenise",          ZOP_TOKENISE,          5,6, 2,4, 0 },
	{ "encode_text",       ZOP_ENCODE_TEXT,       5,6, 4,4, 0 },
	{ "copy_table",        ZOP_COPY_TABLE,        5,6, 3,3, 0 },
	{ "print_table",       ZOP_PRINT_TABLE,       5,6, 3,4, 0 },
	{ "check_arg_count",   ZOP_CHECK_ARG_COUNT,   5,6, 1,1, OPFLAG_JUMP },

	/* extended form */

	{ "save",              ZOP_SAVE,              5,6, 0,4, OPFLAG_ZSTORE },
	{ "restore",           ZOP_RESTORE,           5,6, 0,4, OPFLAG_ZSTORE },
	{ "log_shift",         ZOP_LOG_SHIFT,         5,6, 2,2, OPFLAG_ZSTORE },
	{ "art_shift",         ZOP_ART_SHIFT,         5,6, 2,2, OPFLAG_ZSTORE },
	{ "set_font",          ZOP_SET_FONT,          5,6, 1,2, OPFLAG_ZSTORE },
	{ "draw_picture",      ZOP_DRAW_PICTURE,      6,6, 3,3, 0 },
	{ "picture_data",      ZOP_PICTURE_DATA,      6,6, 2,2, OPFLAG_JUMP },
	{ "erase_picture",     ZOP_ERASE_PICTURE,     6,6, 3,3, 0 },
	{ "set_margins",       ZOP_SET_MARGINS,       6,6, 3,3, 0 },
	{ "save_undo",         ZOP_SAVE_UNDO,         5,6, 0,0, OPFLAG_ZSTORE },
	{ "restore_undo",      ZOP_RESTORE_UNDO,      5,6, 0,0, OPFLAG_ZSTORE },
	{ "print_unicode",     ZOP_PRINT_UNICODE,     5,6, 1,1, 0 },
	{ "check_unicode",     ZOP_CHECK_UNICODE,     5,6, 1,1, OPFLAG_ZSTORE },
	{ "set_true_colour",   ZOP_SET_TRUE_COLOUR,   5,6, 2,3, 0 },

	{ "move_window",       ZOP_MOVE_WINDOW,       6,6, 3,3, 0 },
	{ "window_size",       ZOP_WINDOW_SIZE,       6,6, 3,3, 0 },
	{ "window_style",      ZOP_WINDOW_STYLE,      6,6, 3,3, 0 },
	{ "get_wind_prop",     ZOP_GET_WIND_PROP,     6,6, 2,2, OPFLAG_ZSTORE },
	{ "scroll_window",     ZOP_SCROLL_WINDOW,     6,6, 2,2, 0 },
	{ "pop_stack",         ZOP_POP_STACK,         6,6, 1,2, 0 },
	{ "read_mouse",        ZOP_READ_MOUSE,        6,6, 1,1, 0 },
	{ "mouse_window",      ZOP_MOUSE_WINDOW,      6,6, 1,1, 0 },
	{ "push_stack",        ZOP_PUSH_STACK,        6,6, 1,2, OPFLAG_JUMP },
	{ "put_wind_prop",     ZOP_PUT_WIND_PROP,     6,6, 3,3, 0 },
	{ "print_form",        ZOP_PRINT_FORM,        6,6, 1,1, 0 },
	{ "make_menu",         ZOP_MAKE_MENU,         6,6, 2,2, OPFLAG_JUMP },
	{ "picture_table",     ZOP_PICTURE_TABLE,     6,6, 1,1, 0 },
	{ "buffer_screen",     ZOP_BUFFER_SCREEN,     6,6, 1,1, OPFLAG_ZSTORE },

	// end of list
	{ NULL, 0, 0, 0, 0, 0, 0 }
};

const struct Z_OpcodeInfo * Z_LookupOp (const char * name, bool ignore_version)
{
	// WISH: might be faster to use a symbol table to find opcodes,
	//       however it is very possible to not provide much gain,
	//       so benchmark it first on a large real-world example.

	// for opcodes, Z7 and Z8 are equivalent to Z5
	int version = z_version;
	if (version > 6)
		version = 5;

	int i;
	for (i = 0 ; z_opcodes[i].name != NULL ; i++)
	{
		const struct Z_OpcodeInfo * info = &z_opcodes[i];

		if (! ignore_version)
		{
			if (version < info->version_low)  continue;
			if (version > info->version_high) continue;
		}

		if (strcmp (info->name, name) == 0)
			return info;
	}

	// not found
	return NULL;
}

enum Z_InstructionForm Z_CalcForm (const struct Z_OpcodeInfo * info)
{
	if ((info->op & 0x400) != 0)
		return ZFORM_Extended;

	if ((info->op & 0xFF) <= 0x7F)
		return ZFORM_Long;

	if ((info->op & 0xFF) <= 0xAF)
		return ZFORM_Short;

	if ((info->op & 0xFF) <= 0xBF)
		return ZFORM_Empty;

	return ZFORM_Multi;
}
