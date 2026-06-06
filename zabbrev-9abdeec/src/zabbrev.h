/*
 * Copyright (C) 2021-2024 Henrik Åsman
 * Copyright (C) 2025, 2026 Jason Self <j@jxself.org>
 *
 * This file is part of ZAbbrev.
 *
 * ZAbbrev is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * ZAbbrev is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ZAbbrev. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZABBREV_H
#define ZABBREV_H

#include "util.h"

/* ---- Constants ---- */
#define NUMBER_OF_ABBREVIATIONS  96
#define SPACE_REPLACEMENT        ' '  /* 0x20 */
#define QUOTE_REPLACEMENT        '~'  /* 0x7E */
#define LF_REPLACEMENT           '^'  /* 0x5E */
#define ABBREVIATION_REF_COST    2
#define NUMBER_OF_PASSES_DEFAULT 10000
#define NUMBER_OF_DEEP_PASSES_DEFAULT 1000
#define CUTOFF_LONG_PATTERN      20
#define GSA_SEPARATOR            '\v' /* 0x0B */

/* ---- Encoding types ---- */
#define ENC_NONE   0
#define ENC_ASCII  1
#define ENC_UTF8   2
#define ENC_LATIN1 3

/* ---- GameText ---- */
struct game_text {
    char *text;            /* The text string (owned, dynamically allocated) */
    int text_length;       /* Length of text */
    int *suffix_array;     /* Per-line suffix array (owned) */
    int *lcp_array;        /* Per-line LCP array (owned) */
    int *minimal_cost;     /* Wagner's f/F array (pre-allocated, text_length+2) */
    int *picked_abbrevs;   /* Picked abbreviation indices (text_length+1) */
    int cost_default_alpha;
    int cost_custom_alpha;
    int latest_minimal_cost;
    int latest_rounding_cost;
    int latest_total_bytes;
    int rounding_number;   /* 3 for inline, 6 for v4-7, 12 for v8 */
    int routine_id;
    int packed_address;    /* boolean: is this a packed-address (high) string? */
    int object_description;/* boolean */
};

void game_text_init(struct game_text *gt, const char *text);
void game_text_free(struct game_text *gt);

/* ---- GameText dynamic array ---- */
struct game_text_array {
    struct game_text *data;
    size_t len;
    size_t cap;
};

void gt_array_init(struct game_text_array *a);
void gt_array_push(struct game_text_array *a, struct game_text *gt);
void gt_array_free(struct game_text_array *a);

/* ---- PatternData ---- */
struct pattern_data {
    char *key;             /* The pattern string (owned) */
    int key_len;           /* Length of key */
    int frequency;
    int cost;
    int savings;
    struct int_array *occurrences; /* Array of int_arrays, one per game_text line */
    int occurrences_count;         /* Number of game_text lines */
    int line_of_first;
};

/* Score = frequency * (cost - 2) - ((cost + 2) / 3) * 3 */
int pattern_score(const struct pattern_data *pd);

void pattern_data_init(struct pattern_data *pd);
void pattern_data_free(struct pattern_data *pd);
struct pattern_data *pattern_data_clone(const struct pattern_data *pd);
void pattern_update_occurrences(struct pattern_data *pd,
                                const struct game_text_array *gta,
                                int force_recalc);

/* ---- Max-Heap ---- */
struct max_heap {
    struct pattern_data **items;
    size_t len;
    size_t cap;
};

void heap_init(struct max_heap *h);
void heap_push(struct max_heap *h, struct pattern_data *pd);
struct pattern_data *heap_pop(struct max_heap *h);
struct pattern_data *heap_peek(const struct max_heap *h);
void heap_free(struct max_heap *h);

/* ---- Simple string hash set ---- */
struct hash_entry {
    char *key;
    struct pattern_data *value; /* NULL for sets */
    struct hash_entry *next;
};

struct hash_table {
    struct hash_entry **buckets;
    size_t num_buckets;
    size_t count;
};

void hash_init(struct hash_table *ht, size_t initial_size);
int hash_contains(const struct hash_table *ht, const char *key);
struct pattern_data *hash_get(const struct hash_table *ht, const char *key);
void hash_set(struct hash_table *ht, const char *key, struct pattern_data *value);
void hash_insert_set(struct hash_table *ht, const char *key);
void hash_free(struct hash_table *ht);

/* ---- Char frequency map ---- */
struct char_freq {
    int counts[256];
};

void char_freq_init(struct char_freq *cf);
void char_freq_add(struct char_freq *cf, unsigned char c);

/* ---- Alphabet ---- */
struct alphabet_state {
    char a0[27];
    char a1[27];
    char a2[24];
    int a0_lookup[256];  /* 1 if char is in A0 (including space replacement) */
    int a1a2_lookup[256]; /* 1 if char is in A1 or A2 (including quote/LF replacements) */
    int custom;
};

void alphabet_init(struct alphabet_state *alpha);
void alphabet_build_lookup(struct alphabet_state *alpha);

/* ---- Z-string cost functions ---- */
int zchar_cost(const struct alphabet_state *alpha, unsigned char c);
int zstring_cost(const struct alphabet_state *alpha, const char *s, int len);

/* ---- Optimal parse ---- */
int rescore_optimal_parse(struct game_text_array *game_text,
                          struct pattern_data **abbreviations,
                          int abbrev_count,
                          int return_total_bytes,
                          int zversion,
                          const struct alphabet_state *alpha,
                          const struct int_array *routine_sizes);

/* ---- Main algorithm ---- */
void search_for_abbreviations(const char *game_directory,
                              int inform6_style,
                              int throw_back_lowscorers,
                              int print_debug,
                              int verbose_flag,
                              int quiet_flag,
                              int only_refactor,
                              int compression_level,
                              int number_of_abbrevs,
                              int number_of_passes,
                              int number_of_deep_passes,
                              int z_version,
                              int try_auto_detect,
                              int output_format,
                              int text_encoding,
                              struct alphabet_state *alpha,
                              const char *infodump_filename,
                              const char *txd_filename);

/* ---- Output functions ---- */
void print_abbreviations(struct pattern_data **abbrev_list, int count,
                         const char *game_filename, int to_error,
                         int number_of_abbrevs);
void print_abbreviations_i6(struct pattern_data **abbrev_list, int count,
                            int to_error, int number_of_abbrevs);
void print_alphabet(const struct alphabet_state *alpha);
void print_alphabet_i6(const struct alphabet_state *alpha);
char *format_abbreviation(const char *abbreviation);

/* Sort abbreviations for output. Returns malloc'd array of cloned PatternData*. */
struct pattern_data **abbrev_sort(struct pattern_data **abbrev_list,
                                 int count, int number_of_abbrevs,
                                 int sort_by_frequency);

/* ---- File utilities ---- */
int is_file_utf8(const char *filename);
unsigned char *read_file_bytes(const char *filename, size_t *out_len);

/* ---- String find helper ---- */
int str_find(const char *haystack, int haystack_len,
             const char *needle, int needle_len, int start);

#endif /* ZABBREV_H */
