// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"


#define HASH_SLOTS  4007u
#define HASH_BITS   12


struct StringEntry
{
	const char * str;
};

struct StringSlot
{
	unsigned int  count;
	unsigned int  spare;

	struct StringEntry * entries;
};

static struct StringSlot  the_slots[HASH_SLOTS];


static void STR_RawAdd (struct StringSlot * slot, const char * copy_str)
{
	if (slot->spare == 0)
	{
		// grow the entries in this hash slot
		unsigned int new_count = slot->count + slot->count / 2 + 2;

		if (new_count > (1 << (30 - HASH_BITS)))
			Fatal_Error ("string table overflow\n");

		struct StringEntry * new_entries = UT_Alloc (new_count * sizeof(struct StringEntry));

		if (slot->entries != NULL)
		{
			memcpy (new_entries, slot->entries, slot->count * sizeof(struct StringEntry));
			UT_Free (slot->entries);
		}

		slot->entries = new_entries;
		slot->spare   = new_count - slot->count;
	}

	slot->entries[slot->count].str = copy_str;
	slot->count += 1;
	slot->spare -= 1;
}

uint32_t STR_Hash (const char * str)
{
	uint32_t hash = 0;

	while (*str != 0)
	{
		uint32_t ch = *str++;
		hash = (hash << 5) - hash + ch;
		hash = hash ^ (hash >> 24);
	}

	return hash;
}

// find the string in the string table and return its index.
// when the string is not in the table yet, a _copy_ is added.
int STR_Intern (const char * str)
{
	Assert (str != NULL);

	if (str[0] == 0)
		return 0;

	int slot_idx  = STR_Hash (str) % HASH_SLOTS;
	int entry_idx = 0;

	Assert (slot_idx >= 0);

	struct StringSlot * slot = &the_slots[slot_idx];
	int count = slot->count;

	for (; entry_idx < count ; entry_idx++)
		if (strcmp (slot->entries[entry_idx].str, str) == 0)
			goto found_it;

	STR_RawAdd (slot, UT_Strdup (str));

	found_it:
	{
		int index = slot_idx + (entry_idx << HASH_BITS);
		return index + 1;
	}
}

const char * STR_Get (int index)
{
	if (index == 0)
		return "";

	index = index - 1;

	int slot_idx  = index & ((1 << HASH_BITS) - 1);
	int entry_idx = index >> HASH_BITS;

	return the_slots[slot_idx].entries[entry_idx].str;
}

const char * STR_Copy (const char * str)
{
	return STR_Get (STR_Intern (str));
}
