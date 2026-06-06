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

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

/* Safe allocation wrappers -- abort on failure */
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Error reporting */
void fatal(const char *fmt, ...);
void fatal_errno(const char *fmt, ...);

/* ---- Dynamic int array ---- */
struct int_array {
    int *data;
    size_t len;
    size_t cap;
};

void int_array_init(struct int_array *a);
void int_array_push(struct int_array *a, int val);
void int_array_clear(struct int_array *a);
void int_array_free(struct int_array *a);

/* ---- Dynamic long array ---- */
struct long_array {
    long *data;
    size_t len;
    size_t cap;
};

void long_array_init(struct long_array *a);
void long_array_push(struct long_array *a, long val);
void long_array_free(struct long_array *a);

#endif /* UTIL_H */
