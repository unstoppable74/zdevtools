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
#include <stdarg.h>
#include <errno.h>
#include "util.h"

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p && size) {
        fprintf(stderr, "zabbrev: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *
xcalloc(size_t nmemb, size_t size)
{
    void *p = calloc(nmemb, size);
    if (!p && nmemb && size) {
        fprintf(stderr, "zabbrev: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p && size) {
        fprintf(stderr, "zabbrev: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

char *
xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *p = (char *)xmalloc(len);
    memcpy(p, s, len);
    return p;
}

char *
xstrndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    char *p;
    if (len > n) len = n;
    p = (char *)xmalloc(len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

void
fatal(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "zabbrev: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

void
fatal_errno(const char *fmt, ...)
{
    int e = errno;
    va_list ap;
    fprintf(stderr, "zabbrev: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(e));
    exit(EXIT_FAILURE);
}

/* ---- int_array ---- */

void
int_array_init(struct int_array *a)
{
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

void
int_array_push(struct int_array *a, int val)
{
    if (a->len >= a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        a->data = (int *)xrealloc(a->data, a->cap * sizeof(int));
    }
    a->data[a->len++] = val;
}

void
int_array_clear(struct int_array *a)
{
    a->len = 0;
}

void
int_array_free(struct int_array *a)
{
    free(a->data);
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

/* ---- long_array ---- */

void
long_array_init(struct long_array *a)
{
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

void
long_array_push(struct long_array *a, long val)
{
    if (a->len >= a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        a->data = (long *)xrealloc(a->data, a->cap * sizeof(long));
    }
    a->data[a->len++] = val;
}

void
long_array_free(struct long_array *a)
{
    free(a->data);
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}
