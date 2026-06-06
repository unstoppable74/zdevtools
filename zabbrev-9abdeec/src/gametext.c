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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "zabbrev.h"
#include "suffix_array.h"

/* ---- GameText ---- */

void
game_text_init(struct game_text *gt, const char *text)
{
    int len = (int)strlen(text);
    gt->text = xstrdup(text);
    gt->text_length = len;
    gt->suffix_array = NULL;
    gt->lcp_array = NULL;
    gt->minimal_cost = (int *)xcalloc((size_t)(len + 2), sizeof(int));
    gt->picked_abbrevs = (int *)xmalloc((size_t)(len + 1) * sizeof(int));
    gt->cost_default_alpha = 0;
    gt->cost_custom_alpha = 0;
    gt->latest_minimal_cost = 0;
    gt->latest_rounding_cost = 0;
    gt->latest_total_bytes = 0;
    gt->rounding_number = 3;
    gt->routine_id = -1;
    gt->packed_address = 0;
    gt->object_description = 0;
}

void
game_text_free(struct game_text *gt)
{
    free(gt->text);
    free(gt->suffix_array);
    free(gt->lcp_array);
    free(gt->minimal_cost);
    free(gt->picked_abbrevs);
}

/* ---- GameText array ---- */

void
gt_array_init(struct game_text_array *a)
{
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

void
gt_array_push(struct game_text_array *a, struct game_text *gt)
{
    if (a->len >= a->cap) {
        a->cap = a->cap ? a->cap * 2 : 64;
        a->data = (struct game_text *)xrealloc(a->data,
                   a->cap * sizeof(struct game_text));
    }
    a->data[a->len++] = *gt;
}

void
gt_array_free(struct game_text_array *a)
{
    size_t i;
    for (i = 0; i < a->len; i++)
        game_text_free(&a->data[i]);
    free(a->data);
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

/* ---- PatternData ---- */

void
pattern_data_init(struct pattern_data *pd)
{
    pd->key = NULL;
    pd->key_len = 0;
    pd->frequency = 0;
    pd->cost = 0;
    pd->savings = 0;
    pd->occurrences = NULL;
    pd->occurrences_count = 0;
    pd->line_of_first = -1;
}

void
pattern_data_free(struct pattern_data *pd)
{
    int i;
    free(pd->key);
    if (pd->occurrences) {
        for (i = 0; i < pd->occurrences_count; i++)
            int_array_free(&pd->occurrences[i]);
        free(pd->occurrences);
    }
}

struct pattern_data *
pattern_data_clone(const struct pattern_data *pd)
{
    struct pattern_data *clone = (struct pattern_data *)xmalloc(sizeof(struct pattern_data));
    clone->key = xstrdup(pd->key);
    clone->key_len = pd->key_len;
    clone->frequency = pd->frequency;
    clone->cost = pd->cost;
    clone->savings = pd->savings;
    clone->occurrences = NULL;
    clone->occurrences_count = 0;
    clone->line_of_first = pd->line_of_first;
    return clone;
}

int
pattern_score(const struct pattern_data *pd)
{
    return (pd->frequency * (pd->cost - 2)) - ((pd->cost + 2) / 3) * 3;
}

/* str_find: find needle in haystack starting at start. Return index or -1. */
int
str_find(const char *haystack, int haystack_len,
         const char *needle, int needle_len, int start)
{
    int i;
    if (needle_len <= 0 || start + needle_len > haystack_len) return -1;
    for (i = start; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, (size_t)needle_len) == 0)
            return i;
    }
    return -1;
}

void
pattern_update_occurrences(struct pattern_data *pd,
                           const struct game_text_array *gta,
                           int force_recalc)
{
    int row, col;
    int gt_count = (int)gta->len;

    if (pd->occurrences != NULL && !force_recalc)
        return;

    /* Free old */
    if (pd->occurrences) {
        int i;
        for (i = 0; i < pd->occurrences_count; i++)
            int_array_free(&pd->occurrences[i]);
        free(pd->occurrences);
    }

    pd->occurrences_count = gt_count;
    pd->occurrences = (struct int_array *)xcalloc((size_t)(gt_count + 1),
                                                  sizeof(struct int_array));
    for (row = 0; row < gt_count; row++) {
        int_array_init(&pd->occurrences[row]);
        col = str_find(gta->data[row].text, gta->data[row].text_length,
                       pd->key, pd->key_len, 0);
        while (col >= 0) {
            int_array_push(&pd->occurrences[row], col);
            col = str_find(gta->data[row].text, gta->data[row].text_length,
                           pd->key, pd->key_len, col + 1);
        }
    }
}

/* ---- File utilities ---- */

unsigned char *
read_file_bytes(const char *filename, size_t *out_len)
{
    FILE *f;
    long fsize;
    unsigned char *buf;
    size_t nread;

    f = fopen(filename, "rb");
    if (!f)
        fatal_errno("cannot open '%s'", filename);

    if (fseek(f, 0, SEEK_END) != 0)
        fatal_errno("cannot seek '%s'", filename);
    fsize = ftell(f);
    if (fsize < 0)
        fatal_errno("cannot tell '%s'", filename);
    rewind(f);

    buf = (unsigned char *)xmalloc((size_t)fsize + 1);
    nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);
    *out_len = nread;
    return buf;
}

int
is_file_utf8(const char *filename)
{
    size_t len, i;
    unsigned char *buf = read_file_bytes(filename, &len);
    int is_utf8 = 1;

    /* Check if all bytes form valid UTF-8 sequences */
    i = 0;
    while (i < len) {
        unsigned char c = buf[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (buf[i+1] & 0xC0) != 0x80) { is_utf8 = 0; break; }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len || (buf[i+1] & 0xC0) != 0x80 || (buf[i+2] & 0xC0) != 0x80) { is_utf8 = 0; break; }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len || (buf[i+1] & 0xC0) != 0x80 || (buf[i+2] & 0xC0) != 0x80 || (buf[i+3] & 0xC0) != 0x80) { is_utf8 = 0; break; }
            i += 4;
        } else {
            is_utf8 = 0;
            break;
        }
    }

    free(buf);
    return is_utf8;
}

/* ---- Char frequency ---- */

void
char_freq_init(struct char_freq *cf)
{
    memset(cf->counts, 0, sizeof(cf->counts));
}

void
char_freq_add(struct char_freq *cf, unsigned char c)
{
    cf->counts[c]++;
}

/* ---- Hash table ---- */

static unsigned long
hash_string(const char *s)
{
    unsigned long h = 5381;
    while (*s)
        h = ((h << 5) + h) + (unsigned char)*s++;
    return h;
}

void
hash_init(struct hash_table *ht, size_t initial_size)
{
    ht->num_buckets = initial_size ? initial_size : 4096;
    ht->buckets = (struct hash_entry **)xcalloc(ht->num_buckets,
                                                sizeof(struct hash_entry *));
    ht->count = 0;
}

int
hash_contains(const struct hash_table *ht, const char *key)
{
    unsigned long h = hash_string(key) % ht->num_buckets;
    struct hash_entry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0)
            return 1;
        e = e->next;
    }
    return 0;
}

struct pattern_data *
hash_get(const struct hash_table *ht, const char *key)
{
    unsigned long h = hash_string(key) % ht->num_buckets;
    struct hash_entry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0)
            return e->value;
        e = e->next;
    }
    return NULL;
}

void
hash_set(struct hash_table *ht, const char *key, struct pattern_data *value)
{
    unsigned long h = hash_string(key) % ht->num_buckets;
    struct hash_entry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;
            return;
        }
        e = e->next;
    }
    e = (struct hash_entry *)xmalloc(sizeof(struct hash_entry));
    e->key = xstrdup(key);
    e->value = value;
    e->next = ht->buckets[h];
    ht->buckets[h] = e;
    ht->count++;
}

void
hash_insert_set(struct hash_table *ht, const char *key)
{
    hash_set(ht, key, NULL);
}

void
hash_free(struct hash_table *ht)
{
    size_t i;
    for (i = 0; i < ht->num_buckets; i++) {
        struct hash_entry *e = ht->buckets[i];
        while (e) {
            struct hash_entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    ht->buckets = NULL;
    ht->count = 0;
}

/* ---- Max-Heap (keyed by savings, descending) ---- */

void
heap_init(struct max_heap *h)
{
    h->items = NULL;
    h->len = 0;
    h->cap = 0;
}

/* Compare two heap items: returns >0 if a should be above b (higher priority).
   Primary key: savings (descending). Tie-breaker: key string (ascending). */
static int
heap_cmp(const struct pattern_data *a, const struct pattern_data *b)
{
    if (a->savings != b->savings)
        return (a->savings > b->savings) ? 1 : -1;
    return strcmp(b->key, a->key);
}

static void
heap_sift_up(struct max_heap *h, size_t idx)
{
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (heap_cmp(h->items[idx], h->items[parent]) > 0) {
            struct pattern_data *tmp = h->items[idx];
            h->items[idx] = h->items[parent];
            h->items[parent] = tmp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void
heap_sift_down(struct max_heap *h, size_t idx)
{
    for (;;) {
        size_t largest = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        if (left < h->len && heap_cmp(h->items[left], h->items[largest]) > 0)
            largest = left;
        if (right < h->len && heap_cmp(h->items[right], h->items[largest]) > 0)
            largest = right;
        if (largest != idx) {
            struct pattern_data *tmp = h->items[idx];
            h->items[idx] = h->items[largest];
            h->items[largest] = tmp;
            idx = largest;
        } else {
            break;
        }
    }
}

void
heap_push(struct max_heap *h, struct pattern_data *pd)
{
    if (h->len >= h->cap) {
        h->cap = h->cap ? h->cap * 2 : 256;
        h->items = (struct pattern_data **)xrealloc(h->items,
                    h->cap * sizeof(struct pattern_data *));
    }
    h->items[h->len] = pd;
    heap_sift_up(h, h->len);
    h->len++;
}

struct pattern_data *
heap_pop(struct max_heap *h)
{
    struct pattern_data *top;
    if (h->len == 0) return NULL;
    top = h->items[0];
    h->len--;
    if (h->len > 0) {
        h->items[0] = h->items[h->len];
        heap_sift_down(h, 0);
    }
    return top;
}

struct pattern_data *
heap_peek(const struct max_heap *h)
{
    if (h->len == 0) return NULL;
    return h->items[0];
}

void
heap_free(struct max_heap *h)
{
    free(h->items);
    h->items = NULL;
    h->len = 0;
    h->cap = 0;
}
