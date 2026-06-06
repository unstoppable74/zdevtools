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

#ifndef SUFFIX_ARRAY_H
#define SUFFIX_ARRAY_H

/* Build suffix array for text of length n. Returns malloc'd array of n ints. */
int *build_suffix_array(const char *text, int n);

/* Build LCP array (Kasai's algorithm). Returns malloc'd array of n ints. */
int *build_lcp_array(const char *text, int n, const int *suffix_array);

/* Concatenate texts[] (count strings) separated by '\v'. Returns malloc'd string. */
char *build_generalized_sa_string(const char **texts, int count, int *out_len);

/* Binary search for pattern in suffix array. Returns index or -1. */
int sa_binary_search(const char *text, int text_len, const int *suffix_array,
                     const char *pattern, int pattern_len);

/* Count occurrences using LCP array, given a known suffix array index. */
int sa_count(const int *lcp, int lcp_len, int sa_index, int prefix_len);

/* Find first/last position of prefix in LCP range. */
int sa_find_first_prefix(const int *lcp, int sa_index, int prefix_len);
int sa_find_last_prefix(const int *lcp, int lcp_len, int sa_index, int prefix_len);

/* Count unique patterns from LCP array. */
long sa_count_unique_patterns(const int *lcp, int n);

#endif /* SUFFIX_ARRAY_H */
