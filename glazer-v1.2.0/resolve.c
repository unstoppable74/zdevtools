// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

int section_start[4];
int section_padding[4];

int section_global_vars;
int section_end_of_memory;

static int max_file_size_K;

struct ResolveInfo
{
	// current pass
	int  pass;

	// current section
	enum SectionKind  section;

	// current address
	int  addr;

	// current function, NULL when none
	struct FunctionDef * cur_func;
};

static struct ResolveInfo  resolve;

//----------------------------------------------------------------------

void ResolveInstruction_G (struct Node * ins, struct Node * list);
void ResolveInstruction_Z (struct Node * ins, struct Node * list);
void ResolveFakeIns       (struct Node * ins, struct Node * list);

void ResolveHilos         (struct Node * ins);
void ResolveLabel         (struct Node * ins);
void ResolveFunction      (struct Node * ins);

void ResolveData    (struct Node * ins);
void ResolveReserve (struct Node * ins);
void ResolveAlign   (struct Node * ins);
void ResolveString  (struct Node * ins);


void ResolveSection (struct Node * list, enum SectionKind section, int alignment)
{
	resolve.section  = section;
	resolve.cur_func = NULL;

	section_start[section] = resolve.addr;

	// in Z-code, reserve room for the true entry point code
	if (options.zcode && section == SECTION_TEXT && z_version != 6)
		resolve.addr += 8;

	struct Node * ins;

	for (ins = list->children ; ins != NULL ; ins = ins->next)
	{
		// look-up names, evaluate expressions
		if (! ExpressionsInNode (ins, ECTX_Resolve, resolve.cur_func))
			continue;

		SetErrorPos (ins->pos);

		switch (ins->kind)
		{
			case NI_Opcode:
				if (! options.zcode)
					ResolveInstruction_G (ins, list);
				else
					ResolveInstruction_Z (ins, list);
				break;

			case NI_Label:
				ResolveLabel (ins);
				break;

			case NI_Function:
				ResolveFunction (ins);
				break;

			case ND_Data:
				ResolveData (ins);
				break;

			case ND_Reserve:
				ResolveReserve (ins);
				break;

			case ND_Align:
				ResolveAlign (ins);
				break;

			case ND_String:
				ResolveString (ins);
				break;

			default:
				Fatal_Error ("BUG: unknown node for resolving\n");
		}
	}

	// for Z-code, reserve space for global variables at end of dynamic memory
	if (options.zcode && section == SECTION_DATA)
	{
		section_global_vars = resolve.addr;
		resolve.addr += num_global_vars * 2;
	}

	// determine padding
	int pad = (alignment - (resolve.addr % alignment)) % alignment;

	// ensure Z-code story file is not too small
	if (options.zcode && section == SECTION_DATA && resolve.addr < 256)
		pad = 256 - resolve.addr;

	section_padding[section] = pad;
	resolve.addr += pad;
}

void ResolvePass (int pass, int start_addr)
{
	// TODO: secondary pass for more optimal jumps
	if (pass < 2)
		return;

	resolve.pass = pass;
	resolve.addr = start_addr;

	if (! options.zcode)
	{
		// a little over 2.0GB
		max_file_size_K = 2000 * 1000;

		ResolveSection (stream[SECTION_TEXT], SECTION_TEXT, 256);
		ResolveSection (stream[SECTION_DATA], SECTION_DATA, 256);
		ResolveSection (stream[SECTION_BSS ], SECTION_BSS,  256);

		section_end_of_memory = resolve.addr;

		if (pass == 2)
		{
			if (section_end_of_memory > max_file_size_K * 1024)
				Fatal_Error ("total address space is too large (over 2.0 GB)\n");
		}
	}
	else
	{
		// technically, V6 and V7 can reach 512K, but our code for them
		// is too simplistic (the code/string offsets are always zero).
		max_file_size_K = (z_version < 5) ? 128 : (z_version < 8) ? 256 : 512;

		// make room for the header
		resolve.addr = 64;

		ResolveSection (stream[SECTION_DATA  ], SECTION_DATA,   z_scale_factor);
		ResolveSection (stream[SECTION_STATIC], SECTION_STATIC, z_scale_factor);
		ResolveSection (stream[SECTION_TEXT  ], SECTION_TEXT,   z_scale_factor);

		section_end_of_memory = resolve.addr;

		if (pass == 2)
		{
			if (section_end_of_memory > max_file_size_K * 1024)
				Fatal_Error ("story file size exceeds %dK limit.\n", max_file_size_K);

			if (section_start[SECTION_TEXT] > 65535)
				Fatal_Error ("dynamic + static memory exceeds 64K limit.\n");
		}
	}
}

void ResolveAll (void)
{
	Outputf ("[resolving labels]\n");

	memset (section_start,   0, sizeof(section_start));
	memset (section_padding, 0, sizeof(section_padding));

	// starting address, making room for the header
	int start_addr;
	int pass;

	if (! options.zcode)
	{
		uint32_t rom_start = 9 * 4;
		start_addr = HUFF_MeasureTable (rom_start);
	}
	else
	{
		start_addr = 64;
	}

	for (pass = 1 ; pass <= 2 ; pass++)
	{
		ResolvePass (pass, start_addr);
	}
}

//----------------------------------------------------------------------

void ResolveInstruction_G (struct Node * ins, struct Node * list)
{
	ResolveHilos (ins);

	// handle various "fake" instructions
	ResolveFakeIns (ins, list);

	const struct OpcodeInfo * info = ins->data.gop;
	Assert (info != NULL);

	if (resolve.section == SECTION_BSS)
	{
		PostError ("cannot add instructions to BSS section");
		return;
	}

	if (resolve.cur_func == NULL)
	{
		PostError ("instruction '%s' is outside of a function", info->name);
		return;
	}

	if (resolve.cur_func->section != resolve.section)
	{
		PostError ("instruction '%s' is in wrong section", info->name);
		return;
	}

	// start with the amount needed for the opcode
	int amount = 1;

	if (info->op > 127)
		amount = 2;

	Assert (info->op < 65536);

	// add in the amount for the addressing modes
	int num_operands = Node_Len (ins);

	amount += (num_operands + 1) / 2;

	// finally, add in the data for each operand
	struct Node * T;

	for (T = ins->children ; T != NULL ; T = T->next)
	{
		enum AddressingMode  mode = CalcAddressingMode (T);

		int size = (mode & 3);
		if (size == 3)
			size = 4;

		amount += size;
	}

	resolve.addr += amount;

	// for jumps, we need the PC here to compute relative addressing
	if (info->flags & OPFLAG_JUMP)
	{
		T = Node_Tail (ins);
		Assert (T != NULL);

		if (T->kind == NX_JumpDest)
			T->num = resolve.addr;
	}
}

void ResolveFakeIns (struct Node * ins, struct Node * list)
{
	const struct OpcodeInfo * info = ins->data.gop;
	Assert (info != NULL);

	// handle the fake `dcopy` instruction
	if (info->op == OP_FAKE_DCOPY)
	{
		struct Node * new_ins = Node_New (NI_Opcode, "", ins->pos);

		// redistribute the operands
		struct Node * A1 = Node_Pop (ins);
		struct Node * B1 = Node_Pop (ins);

		struct Node * B2 = Node_Pop (ins);
		struct Node * A2 = Node_Pop (ins);

		// when pushing on stack, need to push low-half first
		if (A2->kind == NX_Push)
		{
			Node_Add (ins, B1);
			Node_Add (ins, B2);

			Node_Add (new_ins, A1);
			Node_Add (new_ins, A2);
		}
		else
		{
			Node_Add (ins, A1);
			Node_Add (ins, A2);

			Node_Add (new_ins, B1);
			Node_Add (new_ins, B2);
		}

		// fix the opcode pointers
		info = LookupOp ("copy");

		    ins->data.gop = info;
		new_ins->data.gop = info;

		Node_AddAfter (list, ins, new_ins);
	}

	// handle the fake `dload` and `dstore` instructions
	if (info->op == OP_FAKE_DLOAD || info->op == OP_FAKE_DSTORE)
	{
		struct Node * new_ins = Node_New (NI_Opcode, "", ins->pos);

		// redistribute the operands
		struct Node * addr1 = Node_Pop (ins);
		struct Node * addr2 = Node_Copy (addr1);

		struct Node * B1 = Node_Pop (ins);
		struct Node * B2 = Node_Pop (ins);

		struct Node * zero = Node_New (NL_Integer, "", ins->pos);
		struct Node * one  = Node_New (NL_Integer, "", ins->pos);

		zero->num = 0;
		 one->num = 1;

		if (info->op == OP_FAKE_DLOAD)
		{
			info = LookupOp ("aload");

			Node_Add (ins, addr1);
			Node_Add (ins, one);
			Node_Add (ins, B1);

			Node_Add (new_ins, addr2);
			Node_Add (new_ins, zero);
			Node_Add (new_ins, B2);
		}
		else
		{
			info = LookupOp ("astore");

			Node_Add (ins, addr1);
			Node_Add (ins, zero);
			Node_Add (ins, B1);

			Node_Add (new_ins, addr2);
			Node_Add (new_ins, one);
			Node_Add (new_ins, B2);
		}

		// fix the opcode pointers
		    ins->data.gop = info;
		new_ins->data.gop = info;

		Node_AddAfter (list, ins, new_ins);
	}

	// handle the fake `dpush` and `dpull` instructions
	if (info->op == OP_FAKE_DPUSH || info->op == OP_FAKE_DPULL)
	{
		struct Node * new_ins = Node_New (NI_Opcode, "", ins->pos);

		// redistribute the operands
		struct Node * B1 = Node_Pop (ins);
		struct Node * B2 = Node_Pop (ins);

		if (info->op == OP_FAKE_DPUSH)
		{
			Node_Add (ins, B2);
			Node_Add (ins, Node_New (NX_Push, "", ins->pos));

			Node_Add (new_ins, B1);
			Node_Add (new_ins, Node_New (NX_Push, "", ins->pos));
		}
		else
		{
			Node_Add (ins, Node_New (NX_Pop, "", ins->pos));
			Node_Add (ins, B2);

			Node_Add (new_ins, Node_New (NX_Pop, "", ins->pos));
			Node_Add (new_ins, B1);
		}

		// fix the opcode pointers
		info = LookupOp ("copy");

		    ins->data.gop = info;
		new_ins->data.gop = info;

		Node_AddAfter (list, ins, new_ins);
	}
}

void ResolveHilos (struct Node * ins)
{
	// find any NX_Hilo nodes and convert them and the following
	// floating point constant into two integer pieces.

	struct Node * T;

	for (T = ins->children ; T != NULL ; T = T->next)
	{
		if (T->kind != NX_Hilo)
			continue;

		struct Node * U = T->next;

		if (U == NULL || ! (U->kind == NL_Float || U->kind == NL_FltSpec))
		{
			PostError ("missing float constant after 'hilo'");
			T->kind = NL_Integer;
			continue;
		}

		uint64_t value;

		if (U->kind == NL_Float)
			value = GetRawDouble (U->data.flt);
		else
			value = GetDoubleSpecial (Node_Str (U));

		T->kind = NL_Integer;
		U->kind = NL_Integer;

		T->num  = (int) (uint32_t) (value >> 32);
		U->num  = (int) (uint32_t) (value);
	}
}

void ResolveLabel (struct Node * ins)
{
	struct LabelDef * lab = ins->data.lab;
	Assert (lab != NULL);

	// check that private labels are in the right section
	const char * name = Node_Str (ins);

	if (name[0] == '.')
	{
		if (resolve.cur_func == NULL)
		{
			PostError ("private label '%s' is outside of a function", name);
			return;
		}

		if (resolve.cur_func->section != resolve.section)
		{
			PostError ("private label '%s' is in wrong section", name);
			return;
		}
	}

	lab->addr    = resolve.addr;
	lab->section = (int)resolve.section;
}

void ResolveFunction (struct Node * ins)
{
	if (resolve.section == SECTION_BSS)
	{
		PostError ("cannot use 'function' in BSS section");
		return;
	}

	struct FunctionDef * fun = ins->data.fun;
	Assert (fun != NULL);

	resolve.cur_func = fun;

	fun->section = resolve.section;

	// locals uses offsets, and each requires 4 bytes, and the offset
	// must fit into 16 bits -- hence this limit.
	int local_limit = 64000 / 4;

	if (options.zcode)
		local_limit = 15;

	if (fun->num_locals > local_limit)
	{
		PostError ("function has too many locals (> %d)", local_limit);
		return;
	}

	// Z_code functions simply begin with a single byte
	if (options.zcode)
	{
		resolve.addr += 1;
		return;
	}

	// there is always one byte for the object type, and two bytes
	// to terminate the local variables.
	int amount = 1 + 2;

	int num_locals = fun->num_locals;

	while (num_locals > 0)
	{
		int use_num = num_locals;
		if (use_num > 255)
			use_num = 255;

		amount     += 2;
		num_locals -= use_num;
	}

	resolve.addr += amount;
}

//----------------------------------------------------------------------

void ResolveInstruction_Z (struct Node * ins, struct Node * list)
{
	(void) list;

	const struct Z_OpcodeInfo * info = ins->data.zop;
	Assert (info != NULL);

	enum Z_InstructionForm form = Z_CalcForm (info);

	if (resolve.section != SECTION_TEXT)
	{
		PostError ("can only use Z-code instructions in TEXT section");
		return;
	}

	if (resolve.cur_func == NULL)
	{
		PostError ("instruction '%s' is outside of a function", info->name);
		return;
	}

	// start with the amount needed for the opcode
	int amount = (form == ZFORM_Extended) ? 2 : 1;

	// add in the amount for the addressing modes
#if 0
	int num_operands = Node_Len (ins);

	amount += (num_operands + 1) / 2;

	// finally, add in the data for each operand
	struct Node * T;

	for (T = ins->children ; T != NULL ; T = T->next)
	{
		enum AddressingMode  mode = CalcAddressingMode (T);

		int size = (mode & 3);
		if (size == 3)
			size = 4;

		amount += size;
	}
#endif

	// handle `print` and `print_ret` text  (FIXME: merged into above loop)
	if (info->flags & OPFLAG_ZTEXT)
	{
		struct Node * T = ins->children;

		// the string should have been converted
		Assert (T->kind == NL_String);
		Assert (T->data.ztx != NULL);

		amount += TextGetLength (T->data.ztx);
	}

	resolve.addr += amount;

	// for jumps, we need the PC here to compute relative addressing
	if (info->flags & OPFLAG_JUMP)
	{
		struct Node * T = Node_Tail (ins);
		Assert (T != NULL);

		if (T->kind == NX_JumpDest)
			T->num = resolve.addr;
	}
}
//----------------------------------------------------------------------

void ResolveData (struct Node * ins)
{
	if (resolve.section == SECTION_BSS)
	{
		PostError ("cannot add data to BSS section");
		return;
	}

	int mul = ins->num;

	struct Node * T;
	for (T = ins->children ; T != NULL ; T = T->next)
	{
		int amount = mul;

		switch (T->kind)
		{
			case NL_Integer:
			case NL_Char:
			case NL_Float:
			case NL_FltSpec:
				// okay
				break;

			case NL_String:
				amount = CharsInString (Node_Str (T)) * mul;
				break;

			case NX_Address:
				// okay
				break;

			default:
				PostError ("weird data element");
				return;
		}

		resolve.addr += amount;
	}
}

void ResolveReserve (struct Node * ins)
{
	struct Node * V = ins->children;

	if (V->kind == NX_Address)
	{
		PostError ("reserve amount cannot depend on a label");
		return;
	}
	else if (V->kind != NL_Integer)
	{
		PostError ("expected an integer for reserve amount");
		return;
	}

	int64_t amount = (int64_t)V->num * (int64_t)ins->num;

	if (amount < 1 || amount > 0x3FFFFFFFll)
	{
		PostError ("illegal quantity to reserve (%lld bytes)", (long long)amount);
		return;
	}

	int64_t new_addr = (int64_t)resolve.addr + amount;

	// this causes a fatal error later on, but for now we just need
	// to prevent overflowing a 31-bit signed integer.
	if (new_addr > ((int64_t)max_file_size_K * 1024ll))
		new_addr = ((int64_t)max_file_size_K * 1024ll);

	resolve.addr = (int)new_addr;

	// store amount in main node for the assembly pass
	ins->num = (int)amount;
}

void ResolveAlign (struct Node * ins)
{
	struct Node * V = ins->children;

	if (V->kind == NX_Address)
	{
		PostError ("alignment value cannot depend on a label");
		return;
	}
	else if (V->kind != NL_Integer)
	{
		PostError ("expected an integer for alignment value");
		return;
	}

	int amount = V->num;

	if (amount < 1 || amount > 0x1000000)
	{
		PostError ("illegal alignment value (%d bytes)", amount);
		return;
	}

	int pad = (amount - (resolve.addr % amount)) % amount;

	int64_t new_addr = (int64_t)resolve.addr + pad;

	// this causes a fatal error later on, but for now we just need
	// to prevent overflowing a 31-bit signed integer.
	if (new_addr > ((int64_t)max_file_size_K * 1024ll))
		new_addr = ((int64_t)max_file_size_K * 1024ll);

	resolve.addr = (int)new_addr;

	// remember padding amount for the assembly pass
	ins->num = pad;
}

void ResolveMeasureStr (const char * str, int char_size)
{
	int len = CharsInString (str);
	resolve.addr += len * char_size;
}

void ResolveString (struct Node * ins)
{
	if (resolve.section == SECTION_BSS)
	{
		PostError ("cannot add data to BSS section");
		return;
	}

	enum ObjectType obj_type = (enum ObjectType) ins->num;

	struct Node * child = ins->children;
	Assert (child != NULL);

	if (obj_type == OBJ_HuffString)
	{
		int size = HUFF_MeasureString (child);
		resolve.addr += size;
		return;
	}

	int char_size = (obj_type == OBJ_UnicodeString) ? 4 : 1;

	if (child->kind == NL_String)
	{
		ResolveMeasureStr (Node_Str (child), char_size);
	}
	else if (child->kind == NS_Complex)
	{
		struct Node * T;
		for (T = child->children ; T != NULL ; T = T->next)
			ResolveMeasureStr (Node_Str (T), char_size);
	}
	else
	{
		PostError ("weird '%s' element, expected a string",
			(obj_type == OBJ_LatinString) ? "latstr" : "unistr");
		return;
	}

	// account for the object type / trailing NUL
	resolve.addr += char_size * 2;
}

//----------------------------------------------------------------------

int CharsInString (const char * str)
{
	int count = 0;

	while (*str != 0)
	{
		int len = tweak_utf8_char_length (*str);

		str   += len;
		count += 1;
	}

	return count;
}

int RoundTo256 (int size)
{
	int remainder = size & 255;

	if (remainder > 0)
		size += (256 - remainder);

	return size;
}
