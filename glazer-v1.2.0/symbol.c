// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"


#define SYM_HASH_SIZE  997

struct SymbolTable
{
	struct Definition * slots[SYM_HASH_SIZE];

	// iterator info
	struct Definition * it_pos;
	int                 it_hash;
};


struct SymbolTable * SYM_New (void)
{
	struct SymbolTable * tab = UT_Alloc (sizeof(struct SymbolTable));

	return tab;
}


void SYM_Free (struct SymbolTable * tab)
{
	// free all the definitions too
	int i;
	for (i = 0 ; i < SYM_HASH_SIZE ; i++)
	{
		struct Definition * def  = NULL;
		struct Definition * next = tab->slots[i];

		while (next != NULL)
		{
			def  = next;
			next = next->next;
			UT_Free (def);
		}

		tab->slots[i] = NULL;
	}

	UT_Free (tab);
}


struct Definition * SYM_Find (struct SymbolTable * tab, const char * name)
{
	uint32_t hash = STR_Hash (name) % SYM_HASH_SIZE;

	struct Definition * def;

	for (def = tab->slots[hash] ; def != NULL ; def = def->next)
		if (strcmp (def->name, name) == 0)
			return def;

	// not found
	return NULL;
}


struct Definition * SYM_Add (struct SymbolTable * tab, enum DefKind kind, const char * name)
{
	struct Definition * def = UT_Alloc (sizeof(struct Definition));

	def->kind = kind;
	def->name = STR_Copy (name);

	uint32_t hash = STR_Hash (name) % SYM_HASH_SIZE;

	// link it into the hash table
	def->next = tab->slots[hash];
	tab->slots[hash] = def;

	return def;
}


void SYM_Remove (struct SymbolTable * tab, const char * name)
{
	uint32_t hash = STR_Hash (name) % SYM_HASH_SIZE;

	struct Definition * def  = tab->slots[hash];
	struct Definition * prev = NULL;

	for (; def != NULL ; prev = def, def = def->next)
	{
		if (strcmp (def->name, name) == 0)
		{
			if (prev != NULL)
				prev->next = def->next;
			else
				tab->slots[hash] = def->next;

			UT_Free (def);
			return;
		}
	}
}


void SYM_Begin (struct SymbolTable * tab)
{
	tab->it_pos  = tab->slots[0];
	tab->it_hash = 0;
}


struct Definition * SYM_Next (struct SymbolTable * tab)
{
	for (;;)
	{
		if (tab->it_pos != NULL)
		{
			struct Definition * def = tab->it_pos;
			tab->it_pos = tab->it_pos->next;

			// skip removed entries
			if (def->name[0] == 0)
				continue;

			return def;
		}

		tab->it_hash += 1;

		// reached the end?
		if (tab->it_hash >= SYM_HASH_SIZE)
			return NULL;

		tab->it_pos = tab->slots[tab->it_hash];
	}
}


struct LabelDef * SYM_SortLabels (struct LabelDef * input)
{
	// NOTE: this is a merge sort

	Assert (input != NULL);

	// only a single item?  done!
	if (input->next == NULL)
		return input;

	// split into left and right lists
	struct LabelDef * left  = NULL;
	struct LabelDef * right = NULL;
	struct LabelDef * temp;

	while (input != NULL)
	{
		// move node from input list --> left list
		temp  = input;
		input = input->next;

		temp->next = left;
		left = temp;

		// swap lists
		temp = left;  left = right;  right = temp;
	}

	// recursively sort each side
	left  = SYM_SortLabels (left);
	right = SYM_SortLabels (right);

	// now merge them....

	struct LabelDef * tail = NULL;

	while (left != NULL || right != NULL)
	{
		// pick the lower address
		if (left == NULL || (right != NULL && left->addr > right->addr))
		{
			temp = left;  left = right;  right = temp;
		}

		// remove the node
		temp = left;
		left = left->next;

		// add it to tail of the output list
		temp->next = NULL;

		if (tail == NULL)
			input = temp;
		else
			tail->next = temp;

		tail = temp;
	}

	return input;
}


void SYM_DumpSection (struct SymbolTable * tab, enum SectionKind section, const char * section_name)
{
	printf ("%s\n", section_name);

	// collect all labels in this section
	struct LabelDef * list = NULL;

	int i;
	struct Definition * def;

	for (i = 0 ; i < SYM_HASH_SIZE ; i++)
	{
		for (def = tab->slots[i] ; def != NULL ; def = def->next)
		{
			if (def->kind != DEF_Label)
				continue;

			struct LabelDef * lab = def->u.lab;

			if (lab->section != section)
				continue;

			// update label to the final address
			lab->addr = AddressForLabel (lab);

			// add to list
			lab->next = list;
			list      = lab;
		}
	}

	if (list == NULL)
		return;

	// sort them by address
	list = SYM_SortLabels (list);

	// display them
	for (; list != NULL ; list = list->next)
	{
		printf ("     %06X: %s\n", list->addr, list->name);
	}

	fflush (stdout);
}


void SYM_Dump (struct SymbolTable * tab)
{
	SYM_DumpSection (tab, SECTION_TEXT, ".text");
	SYM_DumpSection (tab, SECTION_DATA, ".data");
	SYM_DumpSection (tab, SECTION_BSS,  ".bss");
}
