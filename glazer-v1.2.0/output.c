// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

struct OutputState
{
	FILE    * fp;
	uint32_t  size;
	uint32_t  checksum;
	int       pending_mode;
};

static struct OutputState  out;

//----------------------------------------------------------------------

void OUT_OpenFile (const char * filename)
{
	memset (&out, 0, sizeof(out));

	Outputf ("[writing %s]\n", filename);

	out.fp = UT_Open (filename, "wb");
	if (out.fp == NULL)
		Fatal_Error ("failed to create file: %s\n", filename);
}

void OUT_CloseFile (void)
{
	Assert (out.fp != NULL);

	// seek to the checksum position and update it
	uint32_t checksum = out.checksum;

	fflush (out.fp);
	fseek  (out.fp, 8 * 4, SEEK_SET);

	OUT_Long (checksum);

	fflush (out.fp);
	fclose (out.fp);

	out.fp = NULL;
}

void OUT_WriteHeader (int data_start, int bss_start, int end_of_memory,
                      int stack_size, int entry_point, int decode_table)
{
	// magic
	OUT_Long (0x476C756C);

	// version
	OUT_Long (0x00030103);

	// ram start
	OUT_Long (data_start);

	// ext start
	OUT_Long (bss_start);

	// end of memory
	OUT_Long (end_of_memory);

	// stack size
	OUT_Long (stack_size);

	// start function
	OUT_Long (entry_point);

	// decoding table
	OUT_Long (decode_table);

	// checksum -- we must write zero here.
	// the real checksum is written back when file is closed.
	OUT_Long (0);
}

//----------------------------------------------------------------------

uint32_t GetRawFloat (float value)
{
	// we assume the host representation of floats is IEEE-754
	union
	{
		float    F;
		uint32_t I;
	} u;

	u.F = value;
	return u.I;
}

uint64_t GetRawDouble (double value)
{
	// we assume the host representation of doubles is IEEE-754
	union
	{
		double   F;
		uint64_t I;
	} u;

	u.F = value;
	return u.I;
}

uint32_t GetFloatSpecial  (const char * name)
{
	if (name[0] == '+')
		return 0x7F800000u;    // +INFINITY
	else if (name[0] == '-')
		return 0xFF800000u;    // -INFINITY
	else
		return 0x7FC00001u;    // NAN (quiet)
}

uint64_t GetDoubleSpecial (const char * name)
{
	if (name[0] == '+')
		return 0x7FF0000000000000ull;  // +INFINITY
	else if (name[0] == '-')
		return 0xFFF0000000000000ull;  // -INFINITY
	else
		return 0x7FF8000000000001ull;  // NAN (quiet)
}

//----------------------------------------------------------------------

void OUT_Byte (uint8_t value)
{
	// updat the checksum
	unsigned int shift = 8 * (3 - (out.size & 3));
	out.checksum += ((uint32_t)value << shift);

	fputc ((int) value, out.fp);

	out.size += 1;
}

void OUT_Word (uint16_t value)
{
	// big endian
	OUT_Byte ((uint8_t) (value >>  8));
	OUT_Byte ((uint8_t) (value      ));
}

void OUT_Long (uint32_t value)
{
	// big endian
	OUT_Byte ((uint8_t) (value >> 24));
	OUT_Byte ((uint8_t) (value >> 16));
	OUT_Byte ((uint8_t) (value >>  8));
	OUT_Byte ((uint8_t) (value      ));
}

void OUT_Quad (uint64_t value)
{
	// big endian
	OUT_Long ((uint32_t) (value >> 32));
	OUT_Long ((uint32_t) (value      ));
}

void OUT_Zeros (int count)
{
	for (; count > 0 ; count--)
	{
		OUT_Byte (0);
	}
}

void OUT_Opcode (int code)
{
	// firstly, ensure no addressing modes are pending
	out.pending_mode = -1;

	if (code < 0x80)
	{
		OUT_Byte (code);
	}
	else if (code < 0x4000)
	{
		OUT_Word (0x8000 + code);
	}
	else
	{
		Fatal_Error ("humungous opcode on the loose!\n");
	}
}

void OUT_AddrMode (enum AddressingMode mode)
{
	if (out.pending_mode < 0)
	{
		// postpone it until we get another or finish
		out.pending_mode = (int)mode;
	}
	else
	{
		OUT_Byte (((int)mode << 4) | out.pending_mode);
		out.pending_mode = -1;
	}
}

// this is just here to ensure that an odd number of operands
// is handled before the operand data is written.
void OUT_OpData (void)
{
	if (out.pending_mode >= 0)
	{
		OUT_Byte ((uint8_t)out.pending_mode);
	}
}
