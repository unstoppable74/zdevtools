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

#include <stdlib.h>
#include <string.h>
#include "suffix_array.h"
#include "util.h"

/* Suffix structure for sorting */
struct suffix {
    int index;
    int rank[2];
};

static int
suffix_compare(const void *a, const void *b)
{
    const struct suffix *sa = (const struct suffix *)a;
    const struct suffix *sb = (const struct suffix *)b;
    if (sa->rank[0] != sb->rank[0])
        return sa->rank[0] - sb->rank[0];
    return sa->rank[1] - sb->rank[1];
}

int *
build_suffix_array(const char *text, int n)
{
    struct suffix *suffixes;
    int *ind;
    int *suffix_arr;
    int k, i;
    const unsigned char *utext = (const unsigned char *)text;

    if (n == 0) {
        return (int *)xcalloc(1, sizeof(int));
    }

    suffixes = (struct suffix *)xmalloc((size_t)n * sizeof(struct suffix));
    ind = (int *)xmalloc((size_t)n * sizeof(int));

    for (i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = utext[i] - (int)'a';
        suffixes[i].rank[1] = (i + 1 < n) ? utext[i + 1] - (int)'a' : -1;
    }

    qsort(suffixes, (size_t)n, sizeof(struct suffix), suffix_compare);

    k = 4;
    while (k < 2 * n) {
        int rank = 0;
        int prev_rank = suffixes[0].rank[0];
        suffixes[0].rank[0] = rank;
        ind[suffixes[0].index] = 0;

        for (i = 1; i < n; i++) {
            if (suffixes[i].rank[0] == prev_rank &&
                suffixes[i].rank[1] == suffixes[i - 1].rank[1]) {
                prev_rank = suffixes[i].rank[0];
                suffixes[i].rank[0] = rank;
            } else {
                prev_rank = suffixes[i].rank[0];
                suffixes[i].rank[0] = ++rank;
            }
            ind[suffixes[i].index] = i;
        }

        for (i = 0; i < n; i++) {
            int nextindex = suffixes[i].index + k / 2;
            suffixes[i].rank[1] = (nextindex < n)
                ? suffixes[ind[nextindex]].rank[0] : -1;
        }

        qsort(suffixes, (size_t)n, sizeof(struct suffix), suffix_compare);
        k *= 2;
    }

    suffix_arr = (int *)xmalloc((size_t)n * sizeof(int));
    for (i = 0; i < n; i++)
        suffix_arr[i] = suffixes[i].index;

    free(ind);
    free(suffixes);
    return suffix_arr;
}

int *
build_lcp_array(const char *text, int n, const int *suffix_array)
{
    int *lcp;
    int *inv;
    int i, k_val;

    lcp = (int *)xcalloc((size_t)n, sizeof(int));
    inv = (int *)xmalloc((size_t)n * sizeof(int));

    for (i = 0; i < n; i++)
        inv[suffix_array[i]] = i;

    k_val = 0;
    for (i = 0; i < n; i++) {
        if (inv[i] > 0) {
            int j = suffix_array[inv[i] - 1];
            while (i + k_val < n && j + k_val < n &&
                   text[i + k_val] == text[j + k_val])
                k_val++;
            lcp[inv[i]] = k_val;
            if (k_val > 0) k_val--;
        } else {
            k_val = 0;
        }
    }

    free(inv);
    return lcp;
}

char *
build_generalized_sa_string(const char **texts, int count, int *out_len)
{
    int total = 0;
    int i, pos;
    char *result;

    for (i = 0; i < count; i++)
        total += (int)strlen(texts[i]) + 1; /* +1 for separator */

    result = (char *)xmalloc((size_t)total + 1);
    pos = 0;
    for (i = 0; i < count; i++) {
        int slen = (int)strlen(texts[i]);
        memcpy(result + pos, texts[i], (size_t)slen);
        pos += slen;
        result[pos++] = '\v'; /* separator */
    }
    result[pos] = '\0';
    *out_len = total;
    return result;
}

static int
string_compare_n(const char *s1, int s1_len, const char *s2, int length)
{
    int i;
    for (i = 0; i < length; i++) {
        int remaining = s1_len - i;
        if (remaining <= 0) return -1;
        if ((unsigned char)s1[i] < (unsigned char)s2[i]) return -1;
        if ((unsigned char)s1[i] > (unsigned char)s2[i]) return 1;
    }
    return 0;
}

int
sa_binary_search(const char *text, int text_len, const int *suffix_array,
                 const char *pattern, int pattern_len)
{
    int lo = 0;
    int hi = text_len - pattern_len;

    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        int cmp = string_compare_n(text + suffix_array[mid],
                                   text_len - suffix_array[mid],
                                   pattern, pattern_len);
        if (cmp == 0) return mid;
        else if (cmp < 0) lo = mid;
        else hi = mid;
    }
    return -1;
}

int
sa_find_first_prefix(const int *lcp, int sa_index, int prefix_len)
{
    int lo = sa_index;
    while (lcp[lo] >= prefix_len && lo > 0)
        lo--;
    if (lo == sa_index) return sa_index;
    return lo + 1;
}

int
sa_find_last_prefix(const int *lcp, int lcp_len, int sa_index, int prefix_len)
{
    int hi = sa_index;
    while (hi < lcp_len - 1 && lcp[hi + 1] >= prefix_len)
        hi++;
    return hi;
}

int
sa_count(const int *lcp, int lcp_len, int sa_index, int prefix_len)
{
    return sa_find_last_prefix(lcp, lcp_len, sa_index, prefix_len) -
           sa_find_first_prefix(lcp, sa_index, prefix_len) + 1;
}

long
sa_count_unique_patterns(const int *lcp, int n)
{
    long nn;
    long lcp_sum = 0;
    int sep_count;
    int i;

    sep_count = sa_count(lcp, n, 0, 1);
    nn = (long)n - sep_count;
    for (i = 0; i < n; i++)
        lcp_sum += lcp[i];
    return (nn * (nn + 1)) / 2 - lcp_sum;
}
