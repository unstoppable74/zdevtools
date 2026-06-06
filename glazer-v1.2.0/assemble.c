// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

static enum SectionKind  asm_section;

struct Z_ReleaseInfo
{
	int    number;
	char   serial  [10];
	char   compiler[10];
};

static struct Z_ReleaseInfo  release;

void InitAssembler (void)
{
	memset (&release, 0, sizeof(release));

	if (options.zcode)
	{
		release.number = 1;

		// read current date & time to produce the default serial number
		int year, month, day, hour, minute;

		if (! UT_CurrentDate (&year, &month, &day, &hour, &minute))
		{
			year = month = day = 0;
		}

		if (year < 0)
			year = 0;

		snprintf (release.serial, sizeof(release.serial),
		          "%02d%02d%02d", year % 100, month, day);

		snprintf (release.compiler, sizeof(release.compiler), "Glazer");
	}
}

bool AssembleSetRelease (int number, const char * serial)
{
	if (number < 0 || number > 65535)
	{
		PostError ("bad release number: %d\n", number);
		return false;
	}

	release.number = number;

	if (serial != NULL)
	{
		if (strlen (serial) != 6)
		{
			PostError ("serial number must contain six characters (got %d)",
			           (int)strlen (serial));
			return false;
		}

		snprintf (release.serial, sizeof(release.serial), "%s", serial);
	}

	return true;
}

bool AssembleSetCompiler (const char * compiler)
{
	if (strlen (compiler) > 8)
	{
		PostError ("compiler string must be eight or less characters (got %d)",
		           (int)strlen (compiler));
		return false;
	}

	snprintf (release.compiler, sizeof(release.compiler), "%s", compiler);
	return true;
}

//----------------------------------------------------------------------

int AddressForLabel (struct LabelDef * lab)
{
	int base = lab->addr;
	Assert (base >= 0);

	return base;
}

//----------------------------------------------------------------------

int ASM_GetConst (const char * name, int default_value)
{
	struct Definition * def = SYM_Find (all_symbols, name);

	if (def == NULL)
		return default_value;

	if (def->kind != DEF_Constant)
		Fatal_Error ("'%s' cannot be a label\n", name);

	struct Node * value = def->u.con->value;

	Assert (value != NULL);
	Assert (value != P_FAIL);

	if (value->kind != NL_Integer)
		Fatal_Error ("illegal '%s' constant\n", name);

	return value->num;
}

int ASM_GetLabel (const char * name, int default_value)
{
	struct Definition * def = SYM_Find (all_symbols, name);

	if (def == NULL || def->kind != DEF_Label)
	{
		if (default_value < 0 || def != NULL)
			Fatal_Error ("missing '%s' label\n", name);

		return default_value;
	}

	Assert (def->u.lab->addr >= 0);

	return AddressForLabel (def->u.lab);
}

void AssembleHeader_Glulx (void)
{
	int stack_size  = ASM_GetConst ("stack_size", 65536);
	int entry_point = ASM_GetLabel ("entry_point", -1);

	if (stack_size < 256)
		Fatal_Error ("value for 'stack_size' is too small\n");

	if (stack_size > (1 << 24))
		Fatal_Error ("value for 'stack_size' is too large\n");

	// the huffman decoding table
	int decode_table = HUFF_TableAddress ();

	OUT_WriteHeader (
		section_start[SECTION_DATA],
		section_start[SECTION_BSS],
		section_end_of_memory,
		stack_size,
		entry_point,
		decode_table);
}

void AssembleHeader_ZCode (void)
{
	int i;

	int entry_point = ASM_GetLabel ("entry_point", -1);
	if (entry_point < 0)
		Fatal_Error ("missing 'entry_point' label\n");

	// use our generated entry point code
	if (z_version != 6)
		entry_point = section_start[SECTION_TEXT];

	// Z-machine version
	OUT_Byte (z_version);

	// Flags 1
	OUT_Byte (ASM_GetConst ("z_header_flags_1", 0));

	// release number
	OUT_Word (release.number);

	// high memory start
	OUT_Word (section_start[SECTION_TEXT]);

	// initial PC
	OUT_Word (entry_point);

	// dictionary start
	OUT_Word (ASM_GetLabel ("z_dictionary", 0));

	// object table
	OUT_Word (ASM_GetLabel ("z_objects", 0));

	// global vars
	OUT_Word (section_global_vars);

	// static memory start
	OUT_Word (section_start[SECTION_STATIC]);

	// Flags 2
	OUT_Word (ASM_GetConst ("z_header_flags_2", 0));

	// serial number
	for (i = 0 ; i < 6 ; i++)
		OUT_Byte (release.serial[i]);

	// abbreviation table
	OUT_Word (ASM_GetLabel ("z_abbreviations", 0));

	// length of file (updated later)
	OUT_Word (0);

	// checksum of file (updated later)
	OUT_Word (0);

	// interpreter number
	OUT_Byte (ASM_GetConst ("z_interpreter_number", 0));

	// interpreter version
	OUT_Byte (ASM_GetConst ("z_interpreter_version", 0));

	// screen and font size (set by the interpreter)
	for (i = 0 ; i < 8 ; i++)
		OUT_Byte (0);

	// routines offset -- always zero
	OUT_Word (0);

	// static strings offset -- always zero
	OUT_Word (0);

	// default colors (set by the interpreter)
	OUT_Word (0);

	// terminator table
	OUT_Word (ASM_GetLabel ("z_terminators", 0));

	// stream 3 stuff (set by the interpreter)
	OUT_Word (0);

	// Z-code specification version (set by the interpreter)
	OUT_Word (0);

	// alphabet table
	OUT_Word (ASM_GetLabel ("z_alphabet", 0));

	// header extension table
	OUT_Word (ASM_GetLabel ("z_header_extension", 0));

	// the last 8 bytes are not significant, usually containing a
	// version string for the compiler / authoring system.
	int len = (int)strlen (release.compiler);

	for (i = 0 ; i < 8 ; i++)
	{
		int k = i + (len - 8);
		OUT_Byte ((k >= 0) ? release.compiler[k] : 0);
	}
}

void AssembleHeader (void)
{
	if (options.zcode)
		AssembleHeader_ZCode ();
	else
		AssembleHeader_Glulx ();
}

void AssembleAll (void)
{
	if (! options.zcode)
	{
		AssembleSection (stream[SECTION_TEXT], SECTION_TEXT);
		AssembleSection (stream[SECTION_DATA], SECTION_DATA);
	}
	else
	{
		AssembleSection (stream[SECTION_DATA  ], SECTION_DATA);
		AssembleSection (stream[SECTION_STATIC], SECTION_STATIC);
		AssembleSection (stream[SECTION_TEXT  ], SECTION_TEXT);
	}
}

//----------------------------------------------------------------------

void ASM_Instruction_G (struct Node * ins);
void ASM_Instruction_Z (struct Node * ins);
void ASM_Operand       (struct Node * T);
void ASM_Function_G    (struct Node * ins);
void ASM_Function_Z    (struct Node * ins);

void ASM_Data     (struct Node * ins);
void ASM_Reserve  (struct Node * ins);
void ASM_String   (struct Node * ins);

void ASM_Z_EntryPoint (void);
void ASM_Z_Globals    (void);


void AssembleSection (struct Node * list, enum SectionKind section)
{
	asm_section = section;

	// for Z-code, create the true entry point
	if (options.zcode && section == SECTION_TEXT && z_version != 6)
		ASM_Z_EntryPoint ();

	struct Node * ins;

	for (ins = list->children ; ins != NULL ; ins = ins->next)
	{
		// look-up names, evaluate expressions
		if (! ExpressionsInNode (ins, ECTX_Assemble, NULL))
			continue;

		SetErrorPos (ins->pos);

		switch (ins->kind)
		{
			case NI_Opcode:
				if (! options.zcode)
					ASM_Instruction_G (ins);
				else
					ASM_Instruction_Z (ins);
				break;

			case NI_Label:
				// already handled
				break;

			case NI_Function:
				if (! options.zcode)
					ASM_Function_G (ins);
				else
					ASM_Function_Z (ins);
				break;

			case ND_Data:
				ASM_Data (ins);
				break;

			case ND_Reserve:
			case ND_Align:
				ASM_Reserve (ins);
				break;

			case ND_String:
				ASM_String (ins);
				break;

			default:
				Fatal_Error ("BUG: unknown node for assembling\n");
		}
	}

	if (options.zcode && section == SECTION_DATA)
		ASM_Z_Globals ();

	OUT_Zeros (section_padding[section]);
}

void ASM_Z_EntryPoint (void)
{
	int entry_point = ASM_GetLabel ("entry_point", 0);

	// create a `call` instruction
	OUT_Byte (ZOP_CALL_VS);
	OUT_Byte (0x3F);
	OUT_Word (entry_point / z_scale_factor);

	// add a `quit` instruction
	OUT_Byte (ZOP_QUIT);

	// padding
	OUT_Zeros (3);
}

void ASM_Z_Globals (void)
{
	// collect all the values first
	uint16_t values[256];
	memset (values, 0, sizeof(values));

	struct Definition * def;

	SYM_Begin (all_symbols);

	while (NULL != (def = SYM_Next (all_symbols)))
	{
		if (def->kind != DEF_Global)
			continue;

		struct GlobalVar * glob = def->u.glob;

		if (glob->raw == NULL)
			continue;

		struct Node * V = Evaluate (glob->raw, ECTX_Assemble, NULL);
		if (V == P_FAIL)
			continue;

		// FIXME: check type of value, range of value

		values[glob->id] = V->num;
	}

	// write the global variables now
	int i;
	for (i = 0 ; i < num_global_vars ; i++)
	{
		OUT_Word (values[0x10 + i]);
	}
}

//----------------------------------------------------------------------

void ASM_Instruction_G (struct Node * ins)
{
	const struct OpcodeInfo * info = ins->data.gop;
	Assert (info != NULL);

	// perform a check on the `getiosys` instruction
	if (info->op == OP_GETIOSYS)
	{
		enum AddressingMode mode1 = CalcAddressingMode (ins->children);
		enum AddressingMode mode2 = CalcAddressingMode (ins->children->next);

		if (mode1 != mode2)
		{
			SetErrorPos (ins->pos);
			PostError ("store operands of 'getiosys' must be same general type");
			return;
		}
	}

	// output the opcode number
	OUT_Opcode (info->op);

	// next do the addressing mode for each operand
	struct Node * T;
	int op_index = 0;

	for (T = ins->children ; T != NULL ; T = T->next, op_index++)
	{
		SetErrorPos (T->pos);

		enum AddressingMode mode = CalcAddressingMode (T);

		// check that the mode makes sense
		if ((mode >= ADR_Const_Byte && mode <= ADR_Const_Long) &&
		    (op_index >= info->loads) &&
		    ! (info->flags & OPFLAG_JUMP))
		{
			PostError ("store operand of '%s' is a constant", info->name);
			return;
		}

		OUT_AddrMode (CalcAddressingMode (T));
	}

	// finally, add in the data for each operand
	OUT_OpData ();

	for (T = ins->children ; T != NULL ; T = T->next)
	{
		// check for deprecated use of copyb/copys.
		// we disallow it because our locals are always 32-bit.
		if ((info->op == OP_COPYB || info->op == OP_COPYS) && T->kind == NX_Local)
		{
			PostError ("cannot use '%s' with a local var", info->name);
			return;
		}

		ASM_Operand (T);
	}
}

void ASM_Function_G (struct Node * ins)
{
	struct FunctionDef * fun = ins->data.fun;
	Assert (fun != NULL);

	// output the object type (C0 or C1)
	OUT_Byte (ins->num);

	// output the format of local vars
	int num_locals = fun->num_locals;

	while (num_locals > 0)
	{
		int use_num = num_locals;
		if (use_num > 255)
			use_num = 255;

		OUT_Byte (4);
		OUT_Byte (use_num);

		num_locals -= use_num;
	}

	// terminate the local vars
	OUT_Word (0);
}

void ASM_Function_Z (struct Node * ins)
{
	struct FunctionDef * fun = ins->data.fun;
	Assert (fun != NULL);

	OUT_Byte (fun->num_locals);
}

//----------------------------------------------------------------------

void ASM_RawDataElem (int64_t value, int size)
{
	switch (size)
	{
		case 1: OUT_Byte ((uint8_t)  value); break;
		case 2: OUT_Word ((uint16_t) value); break;
		case 4: OUT_Long ((uint32_t) value); break;
		case 8: OUT_Quad ((uint64_t) value); break;
	}
}

void ASM_RawString (const char * str, int size)
{
	uchar_t max_char = (1 << 30);

	if (size == 1) max_char = 0xFF;
	if (size == 2) max_char = 0xFFFF;

	while (*str != 0)
	{
		uchar_t ch;
		str += tweak_utf8_decode_char (&ch, str);

		if (ch > max_char)
		{
			PostError ("character U+%04X is too large for %d-bit string",
				ch, (size == 1) ? 8 : 16);
			return;
		}

		ASM_RawDataElem ((int64_t)ch, size);
	}
}

void ASM_RawInteger (int value, int size)
{
	if (size == 1)
	{
		// we allow the full range of a signed OR unsigned 8-bit value
		if (value < -128 || value > 255)
		{
			PostError ("integer '%d' is too large for 8-bit data", value);
			return;
		}
	}
	else if (size == 2)
	{
		// we allow the full range of a signed OR unsigned 16-bit value
		if (value < -32768 || value > 65535)
		{
			PostError ("integer '%d' is too large for 16-bit data", value);
			return;
		}
	}

	ASM_RawDataElem ((int64_t)value, size);
}

void ASM_RawFloat (double value, int size)
{
	if (size < 4)
	{
		PostError ("cannot store floats in %d-bit data", size * 8);
		return;
	}

	if (size == 4)
	{
		// we don't care when the value loses precision, but we make
		// sure that large values can fit into a 32-bit float.

		if (fabs (value) >= 3.4e38)
		{
			PostError ("float '%1.6e' is too large for 32-bit data", value);
			return;
		}

		OUT_Long (GetRawFloat ((float) value));
	}
	else
	{
		OUT_Quad (GetRawDouble (value));
	}
}

void ASM_RawFltSpec (struct Node * T, int size)
{
	if (size < 4)
	{
		PostError ("cannot store floats in %d-bit data", size * 8);
		return;
	}

	const char * name = Node_Str (T);

	if (size == 4)
		OUT_Long (GetFloatSpecial (name));
	else
		OUT_Quad (GetDoubleSpecial (name));
}

void ASM_Data (struct Node * ins)
{
	int size = ins->num;

	struct Node * T;
	for (T = ins->children ; T != NULL ; T = T->next)
	{
		switch (T->kind)
		{
			case NL_Integer:
			case NL_Char:
			case NX_Address:
				ASM_RawInteger (T->num, size);
				break;

			case NL_Float:
				ASM_RawFloat (T->data.flt, size);
				break;

			case NL_FltSpec:
				ASM_RawFltSpec (T, size);
				break;

			case NL_String:
				ASM_RawString (Node_Str (T), size);
				break;

			default:
				PostError ("weird data element");
				return;
		}
	}
}

void ASM_Reserve (struct Node * ins)
{
	OUT_Zeros (ins->num);
}

void ASM_String (struct Node * ins)
{
	enum ObjectType obj_type = (enum ObjectType) ins->num;

	struct Node * child = ins->children;
	const char  * str   = Node_Str (child);

	// handle huffman strings
	if (obj_type == OBJ_HuffString)
	{
		HUFF_WriteString (child);
		return;
	}

	int size = (obj_type == OBJ_UnicodeString) ? 4 : 1;

	// output the object type byte (E0 or E2)
	if (size == 4)
		OUT_Long ((uint32_t)obj_type << 24);
	else
		OUT_Byte ((uint8_t)obj_type);

	if (child->kind == NS_Complex)
	{
		struct Node * T;
		for (T = child->children ; T != NULL ; T = T->next)
			ASM_RawString (Node_Str (T), size);
	}
	else
	{
		ASM_RawString (str, size);
	}

	// output the NUL terminator
	OUT_Zeros (size);
}

//----------------------------------------------------------------------

void ASM_Operand_Jump   (struct Node * T);
void ASM_Operand_Memory (struct Node * T);
void ASM_Operand_Const  (int num);

enum AddressingMode CalcAddressingMode (struct Node * T)
{
	Assert (T->kind != NL_Word);
	Assert (T->kind != NL_String);

	switch (T->kind)
	{
		case NX_Push:
			return ADR_Push;

		case NX_Drop:
			return ADR_Drop;

		case NX_Pop:
			return ADR_Pop;

		case NX_Return:
			return ADR_Const_Byte;

		case NX_Address:
			// always 4 bytes currently
			return ADR_Const_Long;

		case NX_JumpDest:
			return ADR_Const_Word;

		case NX_Local:
			if (T->num < 256)
				return ADR_Local_Byte;
			else
				return ADR_Local_Word;

		case NX_Memory:
			// always 4 bytes currently
			return ADR_Address_Long;

		case NL_Integer:
		case NL_Char:
			if (T->num == 0)
				return ADR_Const_Zero;
			else if (-127 <= T->num && T->num <= 127)
				return ADR_Const_Byte;
			else if (-32767 <= T->num && T->num <= 32767)
				return ADR_Const_Word;
			else
				return ADR_Const_Long;

		case NL_Float:
		case NL_FltSpec:
			return ADR_Const_Long;

		default:
			Fatal_Error ("BUG: unknown operand node\n");
			return ADR_Drop;
	}
}

void ASM_Operand (struct Node * T)
{
	switch (T->kind)
	{
		case NX_Push:
		case NX_Drop:
		case NX_Pop:
			// these don't have any data
			break;

		case NX_Return:
			OUT_Byte ((uint8_t)T->num);
			break;

		case NX_Address:
			OUT_Long ((uint32_t)T->num);
			break;

		case NX_JumpDest:
			ASM_Operand_Jump (T);
			break;

		case NX_Local:
			if (T->num < 256)
				OUT_Byte ((uint8_t)T->num);
			else
				OUT_Word ((uint16_t)T->num);
			break;

		case NX_Memory:
			ASM_Operand_Memory (T);
			break;

		case NL_Integer:
		case NL_Char:
			ASM_Operand_Const (T->num);
			break;

		case NL_Float:
			ASM_RawFloat (T->data.flt, 4);
			break;

		case NL_FltSpec:
			ASM_RawFltSpec (T, 4);
			break;

		default:
			Fatal_Error ("BUG: unknown operand node\n");
			break;
	}
}

void ASM_Operand_Jump (struct Node * T)
{
	Assert (T->children->kind == NX_Address);

	int offset = T->children->num - T->num + 2;

	if (offset < -32767 || offset > 32767)
	{
		PostError ("jump offset is too large (%d)", offset);
		return;
	}

	Assert (offset != 0);
	Assert (offset != 1);

	OUT_Word ((uint16_t)offset);
}

void ASM_Operand_Memory (struct Node * T)
{
	T = T->children;
	Assert (T != NULL);

	int addr;

	if (T->kind == NX_Address || T->kind == NL_Integer)
	{
		addr = T->num;
	}
	else
	{
		PostError ("weird address in []");
		return;
	}

	OUT_Long (addr);
}

void ASM_Operand_Const (int num)
{
	if (num == 0)
	{
		// no data needed
		return;
	}

	if (-127 <= num && num <= 127)
		OUT_Byte ((uint8_t) num);
	else if (-32767 <= num && num <= 32767)
		OUT_Word ((uint16_t) num);
	else
		OUT_Long ((uint32_t) num);
}

//----------------------------------------------------------------------

void ASM_Instruction_Z (struct Node * ins)
{
	const struct Z_OpcodeInfo * info = ins->data.zop;
	Assert (info != NULL);

	struct Node * T;

	enum Z_InstructionForm form = Z_CalcForm (info);

	// determine base opcode number
	if (form == ZFORM_Extended)
		OUT_Byte (0xBE);

	int op = info->op & 0xFF;

	// check addressing modes.  we either modify the opcode,
	// or we store the modes packed into 1/2 bytes.

	if (form == ZFORM_Short)
	{
		// set bits 4/5 to the operand type (ZADR_XXX)
	}
	else if (form == ZFORM_Long)
	{
		// if either operand is ZADR_Large
		//   op += 0xC0;
		//   form = ZFORM_Multi;
		// else
		//   set bit 6 to: 0 when first operand is ZADR_Small, 1 for ZADR_Variable.
		//   set bit 5 similarly but for second operand.
	}

	OUT_Byte (op);

	if (form == ZFORM_Multi || form == ZFORM_Extended)
	{
		// write one or two bytes for the addressing modes
		// (two bits per operand).

		// TODO
	}

#if 0
	// next do the addressing mode for each operand
	int op_index = 0;

	for (T = ins->children ; T != NULL ; T = T->next, op_index++)
	{
		SetErrorPos (T->pos);

		enum AddressingMode mode = CalcAddressingMode (T);

		// check that the mode makes sense
		if ((mode >= ADR_Const_Byte && mode <= ADR_Const_Long) &&
		    (op_index >= info->loads) &&
		    ! (info->flags & OPFLAG_JUMP))
		{
			PostError ("store operand of '%s' is a constant", info->name);
			return;
		}

		OUT_AddrMode (CalcAddressingMode (T));
	}
#endif

	// add in the data for each operand

	for (T = ins->children ; T != NULL ; T = T->next)
	{
		if (T->kind == NL_String)
		{
			Assert (T->data.ztx != NULL);
			TextWriteBlock (T->data.ztx);
		}

//!!		ASM_Z_Operand (T);
	}

	// for branches, write the offset now
	if (info->flags & OPFLAG_JUMP)
	{
		// TODO
	}

	// inline text for `print` and `print_ret` instructions
	if (info->flags & OPFLAG_ZTEXT)
	{
		// TODO
	}
}
