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

#define _GNU_SOURCE  /* for strcasecmp */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "zabbrev.h"
#include "suffix_array.h"

/* ---- Alphabet ---- */

void
alphabet_init(struct alphabet_state *alpha)
{
    strcpy(alpha->a0, "abcdefghijklmnopqrstuvwxyz");
    strcpy(alpha->a1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    strcpy(alpha->a2, "0123456789.,!?_#'/\\-:()");
    alpha->custom = 0;
    memset(alpha->a0_lookup, 0, sizeof(alpha->a0_lookup));
    memset(alpha->a1a2_lookup, 0, sizeof(alpha->a1a2_lookup));
}

void
alphabet_build_lookup(struct alphabet_state *alpha)
{
    int i;
    memset(alpha->a0_lookup, 0, sizeof(alpha->a0_lookup));
    memset(alpha->a1a2_lookup, 0, sizeof(alpha->a1a2_lookup));
    for (i = 0; alpha->a0[i]; i++)
        alpha->a0_lookup[(unsigned char)alpha->a0[i]] = 1;
    alpha->a0_lookup[(unsigned char)SPACE_REPLACEMENT] = 1;
    for (i = 0; alpha->a1[i]; i++)
        alpha->a1a2_lookup[(unsigned char)alpha->a1[i]] = 1;
    for (i = 0; alpha->a2[i]; i++)
        alpha->a1a2_lookup[(unsigned char)alpha->a2[i]] = 1;
    alpha->a1a2_lookup[(unsigned char)QUOTE_REPLACEMENT] = 1;
    alpha->a1a2_lookup[(unsigned char)LF_REPLACEMENT] = 1;
}

int
zchar_cost(const struct alphabet_state *alpha, unsigned char c)
{
    if (alpha->a0_lookup[c]) return 1;
    if (alpha->a1a2_lookup[c]) return 2;
    return 4;
}

int
zstring_cost(const struct alphabet_state *alpha, const char *s, int len)
{
    int cost = 0;
    int i;
    for (i = 0; i < len; i++)
        cost += zchar_cost(alpha, (unsigned char)s[i]);
    return cost;
}

/* ---- Output functions ---- */

char *
format_abbreviation(const char *abbreviation)
{
    size_t len = strlen(abbreviation);
    char *buf = (char *)xmalloc(len * 2 + 3);
    size_t i, pos;

    buf[0] = '"';
    pos = 1;
    for (i = 0; i < len; i++) {
        char c = abbreviation[i];
        if (c == SPACE_REPLACEMENT)
            buf[pos++] = ' ';
        else if (c == QUOTE_REPLACEMENT)
            buf[pos++] = '"';
        else if (c == LF_REPLACEMENT)
            buf[pos++] = '\n';
        else
            buf[pos++] = c;
    }
    buf[pos++] = '"';
    buf[pos] = '\0';
    return buf;
}

static int
compare_by_length_desc(const void *a, const void *b)
{
    const struct pattern_data *pa = *(const struct pattern_data **)a;
    const struct pattern_data *pb = *(const struct pattern_data **)b;
    if (pb->key_len != pa->key_len)
        return pb->key_len - pa->key_len;
    return strcmp(pa->key, pb->key);
}

struct pattern_data **
abbrev_sort(struct pattern_data **abbrev_list, int count,
            int number_of_abbrevs, int sort_by_frequency)
{
    struct pattern_data **result;
    int i, max_count;
    (void)sort_by_frequency; /* Currently only sort by length desc */

    max_count = (count < number_of_abbrevs) ? count : number_of_abbrevs;
    result = (struct pattern_data **)xmalloc((size_t)max_count * sizeof(struct pattern_data *));

    for (i = 0; i < max_count; i++)
        result[i] = pattern_data_clone(abbrev_list[i]);

    qsort(result, (size_t)max_count, sizeof(struct pattern_data *),
          compare_by_length_desc);

    return result;
}

void
print_abbreviations(struct pattern_data **abbrev_list, int count,
                    const char *game_filename, int to_error,
                    int number_of_abbrevs)
{
    FILE *out = to_error ? stderr : stdout;
    int max_idx = number_of_abbrevs - 1;
    int i;

    if (count - 1 < max_idx) max_idx = count - 1;

    fprintf(out, "        ; Frequent words file for %s\n", game_filename);
    fprintf(out, "\n");

    for (i = 0; i <= max_idx; i++) {
        char *fmt = format_abbreviation(abbrev_list[i]->key);
        int pad_extra = (i + 1 < 10) ? 1 : 0;
        int pad_width = (int)(20 + pad_extra - strlen(fmt));
        if (pad_width < 0) pad_width = 0;
        fprintf(out, "        .FSTR FSTR?%d,%-*s; %4dx%d, saved %d\n",
                i + 1, (int)(strlen(fmt)) + pad_width, fmt,
                abbrev_list[i]->frequency, abbrev_list[i]->cost,
                pattern_score(abbrev_list[i]));
        free(fmt);
    }

    fprintf(out, "WORDS::\n");
    for (i = 0; i <= max_idx; i++)
        fprintf(out, "        FSTR?%d\n", i + 1);
    fprintf(out, "\n");
    fprintf(out, "        .ENDI\n");
}

void
print_abbreviations_i6(struct pattern_data **abbrev_list, int count,
                       int to_error, int number_of_abbrevs)
{
    FILE *out = to_error ? stderr : stdout;
    int max_idx = number_of_abbrevs - 1;
    int i;

    if (count - 1 < max_idx) max_idx = count - 1;

    for (i = 0; i <= max_idx; i++) {
        int spaces;
        size_t line_len;
        const char *warning = "";
        char *display;

        /* Build display string with proper replacements */
        line_len = (size_t)abbrev_list[i]->key_len;
        display = (char *)xmalloc(line_len + 1);
        {
            size_t j;
            for (j = 0; j < line_len; j++) {
                char c = abbrev_list[i]->key[j];
                if (c == SPACE_REPLACEMENT) display[j] = ' ';
                else if (c == LF_REPLACEMENT) display[j] = '^';
                else if (c == '~') display[j] = QUOTE_REPLACEMENT;
                else display[j] = c;
            }
            display[line_len] = '\0';
        }

        spaces = 30 - (int)strlen(display);
        if (spaces < 0) spaces = 0;
        if ((int)strlen(display) > 64)
            warning = " Warning: Abbreviation too long. Inform6 limits abbreviationas to max 64 characters.";

        fprintf(out, "Abbreviate \"%s\";%*s! %5dx%2d, saved %5d%s\n",
                display, spaces, "",
                abbrev_list[i]->frequency, abbrev_list[i]->cost,
                pattern_score(abbrev_list[i]), warning);
        free(display);
    }
}

void
print_alphabet(const struct alphabet_state *alpha)
{
    printf(";\"Custom-made alphabet. Insert at beginning of game file.\"\n");
    printf(";\"(Note! Custom-made alphabet are only defined for versions 5 onward.\"\n");
    printf(";\"       Intrepreters may or may not accept it in earlier versions.\"\n");
    printf(";\"       (see Z-Machine Standards Document, %c3.5.5.))\"\n", 0xA7);
    printf("<CHRSET 0 \"%s\">\n", alpha->a0);
    printf("<CHRSET 1 \"%s\">\n", alpha->a1);
    printf("<CHRSET 2 \"\\\"%s\">\n", alpha->a2);
    printf("\n");
}

void
print_alphabet_i6(const struct alphabet_state *alpha)
{
    printf("! Custom-made alphabet. Insert at beginning of game file (see DM4, %c36).\n", 0xA7);
    printf("! (Note! Custom-made alphabet are only defined for versions 5 onward.\n");
    printf("!        Intrepreters may or may not accept it in earlier versions.\n");
    printf("!        (see Z-Machine Standards Document, %c3.5.5.))\n", 0xA7);
    printf("Zcharacter\n");
    printf("    \"%s\"\n", alpha->a0);
    printf("    \"%s\"\n", alpha->a1);
    printf("    \"%s\";\n", alpha->a2);
    printf("\n");
}

/* ---- Helper: case-insensitive file extension check ---- */
static int
has_extension_zap(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ".zap") == 0);
}

static int
str_contains_ci(const char *s, const char *sub)
{
    size_t slen = strlen(s);
    size_t sublen = strlen(sub);
    size_t i;
    if (sublen > slen) return 0;
    for (i = 0; i <= slen - sublen; i++) {
        size_t j;
        int match = 1;
        for (j = 0; j < sublen; j++) {
            if (tolower((unsigned char)s[i+j]) != tolower((unsigned char)sub[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/* ---- Replace double quotes in a string ---- */
static char *
replace_double_quotes(const char *s, int len)
{
    /* Replace ~~ with ~ */
    char *result = (char *)xmalloc((size_t)len + 1);
    int i, j;
    j = 0;
    for (i = 0; i < len; i++) {
        if ((unsigned char)s[i] == QUOTE_REPLACEMENT &&
            i + 1 < len &&
            (unsigned char)s[i + 1] == QUOTE_REPLACEMENT) {
            result[j++] = QUOTE_REPLACEMENT;
            i++; /* skip next */
        } else {
            result[j++] = s[i];
        }
    }
    result[j] = '\0';
    return result;
}

/* ---- Sort alphabet characters (used for custom alphabet) ---- */
#if 0
static void
sort_alphabet_chars(const char *alphabet_in, int in_len,
                    const char *alpha_org, int org_len,
                    char *out, int out_size)
{
    int i, j;
    char *alpha_arr = (char *)xmalloc((size_t)org_len + 1);
    memset(alpha_arr, ' ', (size_t)org_len);
    alpha_arr[org_len] = '\0';

    for (i = 0; i < org_len && i < out_size; i++) {
        if (memchr(alphabet_in, alpha_org[i], (size_t)in_len))
            alpha_arr[i] = alpha_org[i];
    }

    for (i = 0; i < in_len; i++) {
        if (!memchr(alpha_arr, alphabet_in[i], (size_t)org_len)) {
            for (j = 0; j < org_len; j++) {
                if (alpha_arr[j] == ' ') {
                    alpha_arr[j] = alphabet_in[i];
                    break;
                }
            }
        }
    }

    memcpy(out, alpha_arr, (size_t)org_len);
    out[org_len] = '\0';
    free(alpha_arr);
}
#endif

/* ---- Build path helper ---- */
static char *
build_path(const char *dir, const char *file)
{
    size_t dlen = strlen(dir);
    size_t flen = strlen(file);
    char *path = (char *)xmalloc(dlen + flen + 2);
    memcpy(path, dir, dlen);
    if (dlen > 0 && dir[dlen - 1] != '/') {
        path[dlen] = '/';
        dlen++;
    }
    memcpy(path + dlen, file, flen + 1);
    return path;
}

/* ---- File exists helper ---- */
static int
file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static int
dir_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ---- Verbose output helpers ---- */

/*
 * print_finished_text -- Print a game text entry with abbreviation markers.
 *
 * Shows each string with its picked abbreviations wrapped in {braces},
 * along with z-char cost and rounding information.
 * C equivalent of C#'s FinishedText method.
 */
static void
print_finished_text(FILE *out, struct game_text *gt,
                    struct pattern_data **abbreviations, int abbrev_count)
{
    int *picked;
    char *buf;
    size_t buf_size;
    int i, outpos;

    /* Make a working copy of picked_abbrevs to avoid modifying the original */
    picked = (int *)xmalloc((size_t)(gt->text_length + 1) * sizeof(int));
    memcpy(picked, gt->picked_abbrevs,
           (size_t)(gt->text_length + 1) * sizeof(int));

    /* Clean overlapped abbreviations: keep only the first in each chain */
    for (i = 0; i < gt->text_length; i++) {
        if (picked[i] >= 0 && picked[i] < abbrev_count) {
            int alen = abbreviations[picked[i]]->key_len;
            int k;
            for (k = i + 1; k < i + alen && k < gt->text_length; k++)
                picked[k] = -1;
            i += alen - 1;
        }
    }

    /* Allocate output buffer: worst case each char adds {} around it */
    buf_size = (size_t)gt->text_length * 3 + 100;
    buf = (char *)xmalloc(buf_size);
    outpos = 0;

    /* Build display string with {abbreviation} markers */
    for (i = 0; i < gt->text_length; i++) {
        if (picked[i] >= 0 && picked[i] < abbrev_count) {
            int alen = abbreviations[picked[i]]->key_len;
            buf[outpos++] = '{';
            memcpy(buf + outpos, abbreviations[picked[i]]->key, (size_t)alen);
            outpos += alen;
            buf[outpos++] = '}';
            i += alen - 1;
        } else {
            buf[outpos++] = gt->text[i];
        }
    }
    buf[outpos] = '\0';

    fprintf(out, "%3d z-chars + %d unused slot(s) = %3d bytes: \"%s\"\n",
            gt->latest_minimal_cost, gt->latest_rounding_cost,
            gt->latest_total_bytes, buf);

    free(buf);
    free(picked);
}

/*
 * print_refactoring -- Print "Long repeated strings" refactoring suggestions.
 *
 * Drains the refactoring heap and prints estimated byte savings for each
 * pattern, categorised by position type (full, start, end, inside, etc.).
 * C equivalent of the maxHeapRefactor output in C#.
 */
static void
print_refactoring(FILE *out,
                  struct max_heap *refactor_heap,
                  const struct game_text_array *gta,
                  const struct alphabet_state *alpha,
                  int z_version)
{
    if (refactor_heap->len == 0)
        return;

    fprintf(out, "Long repeated strings:\n");

    while (refactor_heap->len > 0) {
        struct pattern_data *pd = heap_pop(refactor_heap);
        int position_inside = 0;
        int cost, padding_cost, split_cost, saving, effective;
        const char *pos_txt;
        size_t i;

        pattern_update_occurrences(pd, gta, 0);

        for (i = 0; i < gta->len; i++) {
            struct game_text *gt = &gta->data[i];
            if (pd->occurrences != NULL &&
                i < (size_t)pd->occurrences_count &&
                pd->occurrences[i].len > 0) {
                if (gt->text_length == pd->key_len &&
                    strcmp(gt->text, pd->key) == 0)
                    position_inside |= 1;
                else if (gt->text_length >= pd->key_len &&
                         strncmp(gt->text, pd->key,
                                 (size_t)pd->key_len) == 0)
                    position_inside |= 2;
                else if (gt->text_length >= pd->key_len &&
                         strcmp(gt->text + gt->text_length - pd->key_len,
                                pd->key) == 0)
                    position_inside |= 4;
                else
                    position_inside |= 8;
                if (gt->object_description)
                    position_inside |= 16;
            }
        }

        pos_txt = "(mixed )";
        if (position_inside == 1) pos_txt = "( full )";
        if (position_inside == 2) pos_txt = "(start )";
        if (position_inside == 4) pos_txt = "( end  )";
        if (position_inside == 8) pos_txt = "(inside)";
        if ((position_inside & 16) == 16) pos_txt = "(object)";

        cost = zstring_cost(alpha, pd->key, pd->key_len);
        effective = pd->frequency - 1;

        padding_cost = 2;
        split_cost = 6;
        if (position_inside == 1) {
            /* full match: no splitting needed */
            padding_cost = 0;
            split_cost = 0;
        } else if (position_inside == 2 || position_inside == 4 ||
                   position_inside == 6) {
            /* start (2), end (4), or start+end (6): one split point */
            padding_cost = 1;
            split_cost = 3;
        }
        if (z_version > 3) padding_cost *= 2;
        if (z_version > 7) padding_cost *= 2;

        /* ceil((effective * cost * 2) / 3.0) using integer arithmetic */
        saving = (effective * cost * 2 + 2) / 3
                 - split_cost * effective - padding_cost;

        if (saving > 0) {
            fprintf(out, "%3dx%3d z-chars (~ %3d bytes), %s \"%s\"\n",
                    pd->frequency, cost, saving, pos_txt, pd->key);
        }
    }

    fprintf(out, "\n");
}

/* ---- Main algorithm ---- */

void
search_for_abbreviations(const char *game_directory,
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
                         const char *txd_filename)
{
    struct game_text_array game_text_list;
    struct hash_table pattern_dict;
    struct char_freq char_freq;
    struct max_heap max_heap, max_heap_refactor;
    struct int_array routine_sizes;
    struct pattern_data **best_candidates = NULL;
    int best_count = 0;
    int best_cap = 0;
    int total_characters = 0;
    int total_savings = 0;
    int latest_total_bytes = 0;
    int previous_savings = 0;
    int previous_total_bytes = 0;
    int current_abbrev = 0;
    int oversample = 0;
    char *game_filename = NULL;
    char *gsa_string = NULL;
    int gsa_len = 0;
    int *sa = NULL;
    int *lcp = NULL;
    int use_txd = 0;
    int routine_id = 0;
    clock_t t_start, t_part;

    /* Init */
    gt_array_init(&game_text_list);
    char_freq_init(&char_freq);
    hash_init(&pattern_dict, 65536);
    heap_init(&max_heap);
    heap_init(&max_heap_refactor);
    int_array_init(&routine_sizes);
    game_filename = xstrdup("");

    t_start = clock();

    if (!quiet_flag) {
        fprintf(stderr, "ZAbbrev %s by Henrik \xc3\x85sman, (c) 2021-2026\n", PACKAGE_VERSION);
        fprintf(stderr, "Highly optimized abbreviations computed efficiently\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Processing files in directory: %s\n", game_directory);
        if (compression_level == 0) fprintf(stderr, "Compression level            : fastest\n");
        if (compression_level == 1) fprintf(stderr, "Compression level            : fast\n");
        if (compression_level == 2) fprintf(stderr, "Compression level            : normal (n = %d)\n", number_of_passes);
        if (compression_level == 3) fprintf(stderr, "Compression level            : maximum (n1 = %d, n2 = %d)\n", number_of_passes, number_of_deep_passes);
        fprintf(stderr, "\n");
        fprintf(stderr, "Progress                               Time (s)\n");
        fprintf(stderr, "--------                               --------\n");
        fprintf(stderr, "%-40s", "Reading file...");
    }

    if (!dir_exists(game_directory)) {
        fprintf(stderr, "ERROR: Can't find specified directory.\n");
        goto cleanup;
    }

    if (infodump_filename[0] || txd_filename[0]) use_txd = 1;

    if (infodump_filename[0]) {
        char *path = build_path(game_directory, infodump_filename);
        if (!file_exists(path)) {
            fprintf(stderr, "ERROR: Can't find infodump-file.\n");
            free(path);
            goto cleanup;
        }
        free(path);
    }
    if (txd_filename[0]) {
        char *path = build_path(game_directory, txd_filename);
        if (!file_exists(path)) {
            fprintf(stderr, "ERROR: Can't find txd-file.\n");
            free(path);
            goto cleanup;
        }
        free(path);
    }

    /***************************************************************
     * Step 1: Reading file(s)
     ***************************************************************/
    t_part = clock();

    if (inform6_style && !use_txd) {
        /* Read Inform6 gametext.txt */
        char *gt_path = build_path(game_directory, "gametext.txt");
        if (file_exists(gt_path)) {
            FILE *f;
            char *linebuf = NULL;
            size_t linecap = 0;
            int object_number = 0;

            if (text_encoding == ENC_NONE) {
                text_encoding = is_file_utf8(gt_path) ? ENC_UTF8 : ENC_LATIN1;
            }

            f = fopen(gt_path, "r");
            if (!f) fatal_errno("cannot open '%s'", gt_path);

            while (1) {
                /* Read line - compatible with C89 getline equivalent */
                int ch;
                size_t line_len = 0;
                if (!linebuf) {
                    linecap = 256;
                    linebuf = (char *)xmalloc(linecap);
                }
                while ((ch = fgetc(f)) != EOF && ch != '\n' && ch != '\r') {
                    if (line_len + 2 > linecap) {
                        linecap *= 2;
                        linebuf = (char *)xrealloc(linebuf, linecap);
                    }
                    linebuf[line_len++] = (char)ch;
                }
                /* If CR, consume a following LF (CRLF) */
                if (ch == '\r') {
                    int next = fgetc(f);
                    if (next != '\n' && next != EOF)
                        ungetc(next, f);
                }
                linebuf[line_len] = '\0';
                if (ch == EOF && line_len == 0) break;

                /* Skip empty lines */
                {
                    size_t ws = 0;
                    while (ws < line_len && (linebuf[ws] == ' ' || linebuf[ws] == '\t')) ws++;
                    if (ws >= line_len) continue;
                }

                if (line_len > 2 && strchr("GVLOSHW", linebuf[0])) {
                    struct game_text gt;
                    int packed = 0;
                    char *text_part;
                    size_t j;

                    /* Replace chars in-place */
                    for (j = 0; j < line_len; j++) {
                        if (linebuf[j] == '^') linebuf[j] = LF_REPLACEMENT;
                        else if (linebuf[j] == '~') linebuf[j] = QUOTE_REPLACEMENT;
                        else if (linebuf[j] == ' ') linebuf[j] = SPACE_REPLACEMENT;
                    }

                    if (strchr("GVS", linebuf[0])) packed = 1;
                    text_part = linebuf + 3; /* Skip "X: " prefix */

                    game_text_init(&gt, text_part);
                    gt.packed_address = packed;
                    if (linebuf[0] == 'O') {
                        gt.object_description = 1;
                        object_number++;
                    }
                    if (linebuf[0] == 'H')
                        gt.routine_id = routine_id;

                    /* Filter out metaclass objects */
                    {
                        int skip = 0;
                        if (object_number < 6 && linebuf[0] == 'O') {
                            /* Check for Class/Object/Routine/String */
                            if (strcmp(text_part, "^Class") == 0 ||
                                strcmp(text_part, "^Object") == 0 ||
                                strcmp(text_part, "^Routine") == 0 ||
                                strcmp(text_part, "^String") == 0) {
                                /* After replacement, ^ is already LF_REPLACEMENT */
                                char tmp[16];
                                sprintf(tmp, "%cClass", LF_REPLACEMENT);
                                if (strcmp(text_part, tmp) == 0) skip = 1;
                                sprintf(tmp, "%cObject", LF_REPLACEMENT);
                                if (strcmp(text_part, tmp) == 0) skip = 1;
                                sprintf(tmp, "%cRoutine", LF_REPLACEMENT);
                                if (strcmp(text_part, tmp) == 0) skip = 1;
                                sprintf(tmp, "%cString", LF_REPLACEMENT);
                                if (strcmp(text_part, tmp) == 0) skip = 1;
                            }
                        }
                        if (!skip)
                            gt_array_push(&game_text_list, &gt);
                        else
                            game_text_free(&gt);
                    }

                    total_characters += (int)strlen(text_part);

                    /* Count char frequencies */
                    for (j = 0; text_part[j]; j++) {
                        unsigned char c = (unsigned char)text_part[j];
                        if (c != LF_REPLACEMENT && c != QUOTE_REPLACEMENT &&
                            c != SPACE_REPLACEMENT && c != 27)
                            char_freq_add(&char_freq, c);
                    }
                } else {
                    /* Check for version info */
                    if (strstr(linebuf, "Compiled Z-machine version") && try_auto_detect) {
                        const char *p = strstr(linebuf, "version");
                        if (p) {
                            p += 8;
                            while (*p == ' ') p++;
                            if (*p >= '1' && *p <= '8')
                                z_version = *p - '0';
                        }
                    }
                    if (strstr(linebuf, "without inline strings size:")) {
                        const char *p = strstr(linebuf, "size:") + 6;
                        int_array_push(&routine_sizes, atoi(p));
                        routine_id++;
                    }
                }
            }
            fclose(f);
            free(linebuf);
        } else {
            fprintf(stderr, "\nERROR: Found no 'gametext.txt' in directory.\n");
            free(gt_path);
            goto cleanup;
        }
        free(gt_path);
    }

    if (!inform6_style && !use_txd) {
        /* Read ZAP files */
        DIR *dir = opendir(game_directory);
        struct dirent *ent;
        if (!dir) fatal_errno("cannot open directory '%s'", game_directory);

        while ((ent = readdir(dir)) != NULL) {
            char *filepath;
            int search_for_34 = 0;
            int search_for_cr = 0;
            int start_pos = 0;
            int packed = 0;
            size_t file_len;
            unsigned char *file_data;

            if (!has_extension_zap(ent->d_name))
                continue;
            if (str_contains_ci(ent->d_name, "_freq"))
                continue;

            filepath = build_path(game_directory, ent->d_name);

            /* Track shortest filename */
            if (game_filename[0] == '\0' ||
                strlen(ent->d_name) < strlen(game_filename)) {
                free(game_filename);
                game_filename = xstrdup(ent->d_name);
            }

            if (text_encoding == ENC_NONE) {
                text_encoding = is_file_utf8(filepath) ? ENC_UTF8 : ENC_LATIN1;
            }

            file_data = read_file_bytes(filepath, &file_len);

            {
                size_t i;
                for (i = 5; i < file_len; i++) {
                    /* Check 5-char opcode window */
                    char opcode[6];
                    int obj_desc = 0;
                    size_t j;

                    if (i < 5) continue;
                    for (j = 0; j < 5; j++)
                        opcode[j] = (char)toupper(file_data[i - 5 + j]);
                    opcode[5] = '\0';

                    if (strcmp(opcode, ".GSTR") == 0) {
                        search_for_34 = 1;
                        packed = 1;
                    }
                    if (strcmp(opcode, ".STRL") == 0) {
                        search_for_34 = 1;
                        packed = 0;
                        obj_desc = 1;
                    }
                    if (strcmp(opcode, "RINTI") == 0) {
                        search_for_34 = 1;
                        packed = 0;
                    }
                    if (strcmp(opcode, "RINTR") == 0) {
                        search_for_34 = 1;
                        packed = 0;
                    }
                    if (strcmp(opcode, ".NEW ") == 0 && try_auto_detect) {
                        z_version = file_data[i] - 48;
                        if (z_version < 4) z_version = 3;
                        if (z_version > 3 && z_version < 8) z_version = 5;
                    }

                    if (search_for_34 && file_data[i] == 34) {
                        start_pos = (int)i;
                        search_for_34 = 0;
                        search_for_cr = 1;
                    }

                    if (search_for_cr
                        && (file_data[i] == 13 || file_data[i] == 10)) {
                        search_for_cr = 0;
                        if ((int)i - start_pos - 2 > 0) {
                            int tlen = (int)i - start_pos - 2;
                            char *temp = (char *)xmalloc((size_t)tlen + 1);
                            char *cleaned;
                            struct game_text gt;
                            int k;

                            for (k = 0; k < tlen; k++) {
                                unsigned char bc = file_data[start_pos + 1 + k];
                                if (bc == 10) bc = LF_REPLACEMENT;
                                if (bc == 32) bc = SPACE_REPLACEMENT;
                                if (bc == 34) bc = QUOTE_REPLACEMENT;
                                temp[k] = (char)bc;
                            }
                            temp[tlen] = '\0';

                            cleaned = replace_double_quotes(temp, tlen);
                            free(temp);

                            game_text_init(&gt, cleaned);
                            gt.packed_address = packed;
                            gt.object_description = obj_desc;
                            gt_array_push(&game_text_list, &gt);
                            total_characters += gt.text_length;

                            for (k = 0; cleaned[k]; k++) {
                                unsigned char c = (unsigned char)cleaned[k];
                                if (c != LF_REPLACEMENT && c != QUOTE_REPLACEMENT &&
                                    c != SPACE_REPLACEMENT && c != 27)
                                    char_freq_add(&char_freq, c);
                            }
                            free(cleaned);
                        }
                    }
                }
            }

            free(file_data);
            free(filepath);
        }
        closedir(dir);
    }

    if (use_txd) {
        /* Read txd/infodump files - simplified version */
        if (infodump_filename[0]) {
            char *id_path = build_path(game_directory, infodump_filename);
            unsigned char *data;
            size_t data_len;
            size_t i;
            int search_for_34 = 0, search_for_cr = 0, start_pos = 0;

            if (text_encoding == ENC_NONE)
                text_encoding = is_file_utf8(id_path) ? ENC_UTF8 : ENC_LATIN1;

            data = read_file_bytes(id_path, &data_len);

            for (i = 13; i < data_len; i++) {
                char window[14];
                size_t j;
                for (j = 0; j < 13 && (i - 13 + j) < data_len; j++)
                    window[j] = (char)toupper(data[i - 13 + j]);
                window[j] = '\0';

                if (strcmp(window, "DESCRIPTION: ") == 0) search_for_34 = 1;
                if (strncmp(window, "Z-CODE VERSIO", 13) == 0 && i + 13 < data_len)
                    z_version = data[i + 13] - 48;
                if (strncmp(window, "INFORM VERSIO", 13) == 0 && i + 18 < data_len) {
                    char tmp[14];
                    for (j = 0; j < 13 && (i + 5 + j) < data_len; j++)
                        tmp[j] = (char)toupper(data[i + 5 + j]);
                    tmp[j] = '\0';
                    if (strstr(tmp, "ZAPF") == NULL)
                        inform6_style = 1;
                }

                if (search_for_34 && data[i] == 34) {
                    start_pos = (int)i;
                    search_for_34 = 0;
                    search_for_cr = 1;
                }

                if (search_for_cr
                    && (data[i] == 13 || data[i] == 10)) {
                    search_for_cr = 0;
                    if ((int)i - start_pos - 2 > 0) {
                        int tlen = (int)i - start_pos - 2;
                        char *temp = (char *)xmalloc((size_t)tlen + 1);
                        char *cleaned;
                        struct game_text gt;
                        int k;

                        for (k = 0; k < tlen; k++) {
                            unsigned char bc = data[start_pos + 1 + k];
                            if (bc == 10 || bc == 94) bc = LF_REPLACEMENT;
                            if (bc == 32) bc = SPACE_REPLACEMENT;
                            if (bc == 34) bc = QUOTE_REPLACEMENT;
                            temp[k] = (char)bc;
                        }
                        temp[tlen] = '\0';

                        cleaned = replace_double_quotes(temp, tlen);
                        free(temp);

                        game_text_init(&gt, cleaned);
                        gt.packed_address = 0;
                        gt.object_description = 1;
                        gt_array_push(&game_text_list, &gt);
                        total_characters += gt.text_length;

                        for (k = 0; cleaned[k]; k++) {
                            unsigned char c = (unsigned char)cleaned[k];
                            if (c != LF_REPLACEMENT && c != QUOTE_REPLACEMENT &&
                                c != SPACE_REPLACEMENT && c != 27)
                                char_freq_add(&char_freq, c);
                        }
                        free(cleaned);
                    }
                }
            }
            free(data);
            free(id_path);
        }

        if (txd_filename[0]) {
            char *txd_path = build_path(game_directory, txd_filename);
            unsigned char *data;
            size_t data_len;
            size_t i;
            int search_for_34 = 0, search_for_cr = 0, start_pos = 0;
            int code_area = 1;
            int packed = 0;

            if (text_encoding == ENC_NONE)
                text_encoding = is_file_utf8(txd_path) ? ENC_UTF8 : ENC_LATIN1;

            data = read_file_bytes(txd_path, &data_len);

            for (i = 9; i < data_len; i++) {
                char window[10];
                size_t j;
                for (j = 0; j < 9 && (i - 9 + j) < data_len; j++)
                    window[j] = (char)toupper(data[i - 9 + j]);
                window[j] = '\0';

                if (code_area && strcmp(window, "PRINT    ") == 0) search_for_34 = 1;
                if (code_area && strcmp(window, "PRINT_RET") == 0) search_for_34 = 1;
                if (strcmp(window, "D OF CODE") == 0) {
                    code_area = 0;
                    search_for_34 = 1;
                    packed = 1;
                }

                if (search_for_34 && data[i] == 34) {
                    start_pos = (int)i;
                    search_for_34 = 0;
                    search_for_cr = 1;
                    i++;
                }
                if (search_for_cr && data[i] == 34) {
                    search_for_cr = 0;
                    if (!code_area) search_for_34 = 1;

                    if ((int)i - start_pos - 1 > 0) {
                        int tlen = (int)i - start_pos - 1;
                        char *temp = (char *)xmalloc((size_t)tlen + 1);
                        char *cleaned;
                        struct game_text gt;
                        int k;
                        /* Remove } chars after cleaning */
                        char *no_brace;
                        int nb_len;

                        for (k = 0; k < tlen; k++) {
                            unsigned char bc = data[start_pos + 1 + k];
                            if (bc == 13) bc = '}';
                            if (bc == 10) bc = 32;
                            if (bc == 94) bc = LF_REPLACEMENT;
                            if (bc == 32) bc = SPACE_REPLACEMENT;
                            if (bc == 34) bc = QUOTE_REPLACEMENT;
                            temp[k] = (char)bc;
                        }
                        temp[tlen] = '\0';

                        cleaned = replace_double_quotes(temp, tlen);
                        free(temp);

                        /* Remove '}' characters */
                        nb_len = 0;
                        no_brace = (char *)xmalloc(strlen(cleaned) + 1);
                        for (k = 0; cleaned[k]; k++) {
                            if (cleaned[k] != '}')
                                no_brace[nb_len++] = cleaned[k];
                        }
                        no_brace[nb_len] = '\0';
                        free(cleaned);

                        game_text_init(&gt, no_brace);
                        gt.packed_address = packed;
                        gt_array_push(&game_text_list, &gt);
                        total_characters += gt.text_length;

                        for (k = 0; no_brace[k]; k++) {
                            unsigned char c = (unsigned char)no_brace[k];
                            if (c != LF_REPLACEMENT && c != QUOTE_REPLACEMENT &&
                                c != SPACE_REPLACEMENT && c != 27)
                                char_freq_add(&char_freq, c);
                        }
                        free(no_brace);
                    }
                }
            }
            free(data);
            free(txd_path);
        }
    }

    /* Adjust output format */
    if (output_format == 1) inform6_style = 1;
    if (output_format == 2) inform6_style = 0;

    {
        double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
        if (!quiet_flag)
            fprintf(stderr, "%7.3f  %d characters\n", elapsed, total_characters);
    }

    /***************************************************************
     * Step 2: Build suffix arrays
     ***************************************************************/
    t_part = clock();
    if (!quiet_flag)
        fprintf(stderr, "%-40s", "Building suffix arrays...");

    /* Build alphabet lookup */
    alphabet_build_lookup(alpha);

    if (alpha->custom) {
        /* Calculate cost with default alphabet first, then custom */
        struct alphabet_state default_alpha;
        size_t gi;
        alphabet_init(&default_alpha);
        alphabet_build_lookup(&default_alpha);
        for (gi = 0; gi < game_text_list.len; gi++)
            game_text_list.data[gi].cost_default_alpha =
                zstring_cost(&default_alpha, game_text_list.data[gi].text,
                             game_text_list.data[gi].text_length);
        for (gi = 0; gi < game_text_list.len; gi++)
            game_text_list.data[gi].cost_custom_alpha =
                zstring_cost(alpha, game_text_list.data[gi].text,
                             game_text_list.data[gi].text_length);
    }

    /* Build per-line suffix arrays and generalized suffix array */
    {
        const char **texts;
        size_t gi;

        texts = (const char **)xmalloc(game_text_list.len * sizeof(const char *));
        for (gi = 0; gi < game_text_list.len; gi++) {
            texts[gi] = game_text_list.data[gi].text;
            game_text_list.data[gi].suffix_array =
                build_suffix_array(game_text_list.data[gi].text,
                                   game_text_list.data[gi].text_length);
            game_text_list.data[gi].lcp_array =
                build_lcp_array(game_text_list.data[gi].text,
                                game_text_list.data[gi].text_length,
                                game_text_list.data[gi].suffix_array);
        }

        gsa_string = build_generalized_sa_string(texts, (int)game_text_list.len, &gsa_len);
        sa = build_suffix_array(gsa_string, gsa_len);
        lcp = build_lcp_array(gsa_string, gsa_len, sa);
        free(texts);
    }

    /* Update rounding numbers */
    {
        size_t gi;
        for (gi = 0; gi < game_text_list.len; gi++) {
            if (game_text_list.data[gi].packed_address) {
                game_text_list.data[gi].rounding_number = 3;
                if (z_version > 3) game_text_list.data[gi].rounding_number = 6;
                if (z_version == 8) game_text_list.data[gi].rounding_number = 12;
            }
        }
    }

    {
        double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
        if (!quiet_flag)
            fprintf(stderr, "%7.3f  %ld potential patterns\n", elapsed,
                    sa_count_unique_patterns(lcp, gsa_len));
    }

    /***************************************************************
     * Step 3: Extract viable patterns
     ***************************************************************/
    t_part = clock();
    if (!quiet_flag)
        fprintf(stderr, "%-40s", "Extracting viable patterns...");

    {
        int lcp_length = gsa_len - 2;
        int i_start = sa_count(lcp, gsa_len, 0, 1);
        int i;

        for (i = i_start; i <= lcp_length; i++) {
            if (lcp[i + 1] > 0) {
                int start = 1;
                int lcp_temp;
                int j;
                if (i > 0) start = lcp[i];
                if (start < 1) start = 1;
                lcp_temp = lcp[i + 1];
                for (j = start; j <= lcp_temp; j++) {
                    /* Extract candidate */
                    char *skey;
                    int has_sep = 0, has_at = 0;
                    int k;
                    int cost, freq;
                    int sa_idx = sa[i];

                    /* Check for separator and @ */
                    for (k = 0; k < j; k++) {
                        if (gsa_string[sa_idx + k] == '\v') { has_sep = 1; break; }
                        if (gsa_string[sa_idx + k] == '@') { has_at = 1; break; }
                    }
                    if (has_sep || has_at) continue;

                    skey = xstrndup(gsa_string + sa_idx, (size_t)j);
                    cost = zstring_cost(alpha, skey, j);
                    freq = sa_count(lcp, gsa_len, i, j);

                    if (freq > 1 && (freq * (cost - 2)) - ((cost + 2) / 3) * 3 > 0) {
                        if (!hash_contains(&pattern_dict, skey)) {
                            struct pattern_data *pd = (struct pattern_data *)xmalloc(sizeof(struct pattern_data));
                            pattern_data_init(pd);
                            pd->key = xstrdup(skey);
                            pd->key_len = j;
                            pd->cost = cost;
                            pd->frequency = freq;
                            hash_set(&pattern_dict, skey, pd);
                        }
                    }
                    free(skey);
                }
            }
        }
    }

    if (game_text_list.len == 0) {
        fprintf(stderr, "ERROR: No data to index.\n");
        goto cleanup;
    }

    {
        double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
        if (!quiet_flag)
            fprintf(stderr, "%7.3f  %lu strings, %lu patterns extracted\n", elapsed,
                    (unsigned long)game_text_list.len, (unsigned long)pattern_dict.count);
    }

    /***************************************************************
     * Step 4: Build max heap with naive score
     ***************************************************************/
    t_part = clock();
    if (!quiet_flag)
        fprintf(stderr, "%-40s", "Build max heap with naive score...");

    {
        struct max_heap length_heap;
        struct hash_table temp_set;
        size_t bi;

        heap_init(&length_heap);
        hash_init(&temp_set, 4096);

        for (bi = 0; bi < pattern_dict.num_buckets; bi++) {
            struct hash_entry *e = pattern_dict.buckets[bi];
            while (e) {
                if (e->value) {
                    e->value->savings = pattern_score(e->value);
                    if (e->value->savings > 0) {
                        if (e->value->key_len <= CUTOFF_LONG_PATTERN) {
                            heap_push(&max_heap, e->value);
                        } else {
                            /* Longer patterns - keyed by length for dedup */
                            struct pattern_data *pd_copy = e->value;
                            pd_copy->savings = pd_copy->key_len; /* temp: sort by len */
                            heap_push(&length_heap, pd_copy);
                        }
                    }
                }
                e = e->next;
            }
        }

        /* Filter long patterns: only keep if not a substring of already seen */
        while (length_heap.len > 0) {
            struct pattern_data *cand = heap_pop(&length_heap);
            char *sub1 = xstrndup(cand->key + 1, (size_t)(cand->key_len - 1));
            char *sub2 = xstrndup(cand->key, (size_t)(cand->key_len - 1));

            hash_insert_set(&temp_set, sub1);
            hash_insert_set(&temp_set, sub2);

            if (!hash_contains(&temp_set, cand->key)) {
                cand->savings = pattern_score(cand);
                heap_push(&max_heap, cand);
                heap_push(&max_heap_refactor, cand);
            }
            free(sub1);
            free(sub2);
        }

        heap_free(&length_heap);
        hash_free(&temp_set);
    }

    {
        double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
        if (!quiet_flag)
            fprintf(stderr, "%7.3f  %lu patterns added to heap\n", elapsed,
                    (unsigned long)max_heap.len);
    }

    /***************************************************************
     * Step 5: Optimal parse - greedy selection
     ***************************************************************/
    if (throw_back_lowscorers) oversample = 5;

    if (!only_refactor) {
        t_part = clock();
        if (!quiet_flag)
            fprintf(stderr, "%-40s", "Rescoring with optimal parse...");

        best_cap = number_of_abbrevs + oversample + 16;
        best_candidates = (struct pattern_data **)xmalloc(
            (size_t)best_cap * sizeof(struct pattern_data *));
        best_count = 0;

        do {
            struct pattern_data *candidate;
            int current_savings, delta_savings;

            if (max_heap.len == 0) break;
            candidate = heap_pop(&max_heap);
            if (best_count >= best_cap) {
                best_cap *= 2;
                best_candidates = (struct pattern_data **)xrealloc(
                    best_candidates,
                    (size_t)best_cap * sizeof(struct pattern_data *));
            }
            best_candidates[best_count] = candidate;
            best_count++;

            current_savings = rescore_optimal_parse(&game_text_list,
                best_candidates, best_count, 0, z_version, alpha,
                &routine_sizes);
            delta_savings = current_savings - previous_savings;

            if (max_heap.len > 0 && delta_savings < heap_peek(&max_heap)->savings) {
                /* Remove and reinsert with new score */
                best_count--;
                candidate->savings = delta_savings;
                heap_push(&max_heap, candidate);
            } else {
                if (print_debug) {
                    char *fmt = format_abbreviation(best_candidates[current_abbrev]->key);
                    fprintf(stderr, "Adding abbrev no %d: %s, Total savings: %d\n",
                            current_abbrev + 1, fmt, current_savings);
                    free(fmt);
                }
                current_abbrev++;
                previous_savings = current_savings;
                total_savings = current_savings;

                if (throw_back_lowscorers) {
                    int latest_saved = current_savings - (previous_savings - delta_savings);
                    int need_recalc = 0;
                    int ii;
                    for (ii = best_count - 2; ii >= 0; ii--) {
                        if (best_candidates[ii]->savings < latest_saved) {
                            heap_push(&max_heap, best_candidates[ii]);
                            /* Shift remaining down */
                            {
                                int jj;
                                for (jj = ii; jj < best_count - 1; jj++)
                                    best_candidates[jj] = best_candidates[jj + 1];
                            }
                            best_count--;
                            current_abbrev--;
                            need_recalc = 1;
                        }
                    }
                    if (need_recalc)
                        previous_savings = rescore_optimal_parse(&game_text_list,
                            best_candidates, best_count, 0, z_version, alpha,
                            &routine_sizes);
                }
            }
        } while (current_abbrev < number_of_abbrevs + oversample && max_heap.len > 0);

        latest_total_bytes = rescore_optimal_parse(&game_text_list,
            best_candidates, best_count, 1, z_version, alpha, &routine_sizes);

        {
            double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
            if (!quiet_flag)
                fprintf(stderr, "%7.3f  Total saving %d z-chars, text size = %d bytes\n",
                        elapsed, total_savings, latest_total_bytes);
        }

        /* Trim to number_of_abbrevs */
        while (best_count > number_of_abbrevs) {
            best_count--;
            heap_push(&max_heap, best_candidates[best_count]);
        }
    }

    /***************************************************************
     * Step 6: Refinement (x2-x3)
     ***************************************************************/
    if (!only_refactor && compression_level > 1) {
        int passes = 0;
        int max_abbrev_length = 0;
        int i;

        t_part = clock();
        if (!quiet_flag)
            fprintf(stderr, "%-40s", "Refining picked abbreviations...");

        previous_total_bytes = latest_total_bytes;
        for (i = 0; i < best_count; i++) {
            if (best_candidates[i]->key_len > max_abbrev_length &&
                best_candidates[i]->key_len <= CUTOFF_LONG_PATTERN)
                max_abbrev_length = best_candidates[i]->key_len;
        }
        max_abbrev_length += 2;

        while (passes < number_of_passes && max_heap.len > 0) {
            struct pattern_data *runnerup = heap_pop(&max_heap);
            if (runnerup->key_len > max_abbrev_length)
                continue;
            passes++;

            if (compression_level > 2 && passes < number_of_deep_passes) {
                /* Maximum: try replacing each existing abbrev */
                int best_delta = 0;
                int best_index = -1;
                int best_total = 0;

                for (i = 0; i < best_count; i++) {
                    struct pattern_data *temp = best_candidates[i];
                    int delta;
                    best_candidates[i] = runnerup;
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha,
                        &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > best_delta) {
                        best_delta = delta;
                        best_index = i;
                        best_total = latest_total_bytes;
                    }
                    best_candidates[i] = temp;
                }
                if (best_index != -1) {
                    previous_total_bytes = best_total;
                    best_candidates[best_index] = runnerup;
                }
            } else {
                /* Normal: only try replacing overlapping abbrevs */
                for (i = 0; i < best_count; i++) {
                    if (strstr(runnerup->key, best_candidates[i]->key) ||
                        strstr(best_candidates[i]->key, runnerup->key)) {
                        struct pattern_data *temp = best_candidates[i];
                        int delta;
                        best_candidates[i] = runnerup;
                        latest_total_bytes = rescore_optimal_parse(&game_text_list,
                            best_candidates, best_count, 1, z_version, alpha,
                            &routine_sizes);
                        delta = previous_total_bytes - latest_total_bytes;
                        if (delta > 0) {
                            previous_total_bytes = latest_total_bytes;
                            break;
                        } else {
                            best_candidates[i] = temp;
                        }
                    }
                }
            }
        }

        latest_total_bytes = previous_total_bytes;
        total_savings = rescore_optimal_parse(&game_text_list,
            best_candidates, best_count, 0, z_version, alpha, &routine_sizes);

        {
            double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
            if (!quiet_flag)
                fprintf(stderr, "%7.3f  Total saving %d z-chars, text size = %d bytes\n",
                        elapsed, total_savings, latest_total_bytes);
        }
    }

    /***************************************************************
     * Step 7: Add/remove initial/trailing space (x1+)
     ***************************************************************/
    if (!only_refactor && compression_level > 0) {
        int iterations;

        t_part = clock();
        if (!quiet_flag)
            fprintf(stderr, "%-40s", "Add/remove initial/trailing space...");

        previous_total_bytes = rescore_optimal_parse(&game_text_list,
            best_candidates, best_count, 1, z_version, alpha, &routine_sizes);

        for (iterations = 1; iterations <= 2; iterations++) {
            int i;
            /* Try removing/adding leading space */
            for (i = 0; i < best_count; i++) {
                int klen = best_candidates[i]->key_len;
                if (best_candidates[i]->key[0] == SPACE_REPLACEMENT && klen > 1) {
                    /* Try removing leading space */
                    char *old_key = best_candidates[i]->key;
                    int delta;
                    best_candidates[i]->key = xstrdup(old_key + 1);
                    best_candidates[i]->key_len = klen - 1;
                    best_candidates[i]->cost -= 1;
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                    } else {
                        free(best_candidates[i]->key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = klen;
                        best_candidates[i]->cost += 1;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }
                } else {
                    /* Try adding leading space */
                    char *old_key = best_candidates[i]->key;
                    char *new_key = (char *)xmalloc((size_t)klen + 2);
                    int delta;
                    new_key[0] = SPACE_REPLACEMENT;
                    memcpy(new_key + 1, old_key, (size_t)klen + 1);
                    best_candidates[i]->key = new_key;
                    best_candidates[i]->key_len = klen + 1;
                    best_candidates[i]->cost += 1;
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                    } else {
                        free(new_key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = klen;
                        best_candidates[i]->cost -= 1;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }
                }
            }

            /* Try removing/adding trailing space */
            for (i = 0; i < best_count; i++) {
                int klen = best_candidates[i]->key_len;
                if (best_candidates[i]->key[klen - 1] == SPACE_REPLACEMENT && klen > 1) {
                    char *old_key = best_candidates[i]->key;
                    int delta;
                    best_candidates[i]->key = xstrndup(old_key, (size_t)(klen - 1));
                    best_candidates[i]->key_len = klen - 1;
                    best_candidates[i]->cost -= 1;
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                    } else {
                        free(best_candidates[i]->key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = klen;
                        best_candidates[i]->cost += 1;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }
                } else {
                    char *old_key = best_candidates[i]->key;
                    char *new_key = (char *)xmalloc((size_t)klen + 2);
                    int delta;
                    memcpy(new_key, old_key, (size_t)klen);
                    new_key[klen] = SPACE_REPLACEMENT;
                    new_key[klen + 1] = '\0';
                    best_candidates[i]->key = new_key;
                    best_candidates[i]->key_len = klen + 1;
                    best_candidates[i]->cost += 1;
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                    } else {
                        free(new_key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = klen;
                        best_candidates[i]->cost -= 1;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }
                }
            }

            /* Try removing initial/trailing one character */
            for (i = 0; i < best_count; i++) {
                if (best_candidates[i]->key_len > 1) {
                    char *old_key = best_candidates[i]->key;
                    int old_len = best_candidates[i]->key_len;
                    int old_cost = best_candidates[i]->cost;
                    int delta;

                    /* Remove first char */
                    best_candidates[i]->key = xstrdup(old_key + 1);
                    best_candidates[i]->key_len = old_len - 1;
                    best_candidates[i]->cost = zstring_cost(alpha, best_candidates[i]->key, best_candidates[i]->key_len);
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                        old_key = best_candidates[i]->key;
                        old_len = best_candidates[i]->key_len;
                        old_cost = best_candidates[i]->cost;
                    } else {
                        free(best_candidates[i]->key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = old_len;
                        best_candidates[i]->cost = old_cost;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }

                    /* Remove last char */
                    if (best_candidates[i]->key_len > 1) {
                        old_key = best_candidates[i]->key;
                        old_len = best_candidates[i]->key_len;
                        old_cost = best_candidates[i]->cost;
                        best_candidates[i]->key = xstrndup(old_key, (size_t)(old_len - 1));
                        best_candidates[i]->key_len = old_len - 1;
                        best_candidates[i]->cost = zstring_cost(alpha, best_candidates[i]->key, best_candidates[i]->key_len);
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                        latest_total_bytes = rescore_optimal_parse(&game_text_list,
                            best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                        delta = previous_total_bytes - latest_total_bytes;
                        if (delta > 0) {
                            previous_total_bytes = latest_total_bytes;
                            free(old_key);
                        } else {
                            free(best_candidates[i]->key);
                            best_candidates[i]->key = old_key;
                            best_candidates[i]->key_len = old_len;
                            best_candidates[i]->cost = old_cost;
                            pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                        }
                    }
                }
            }

            /* Try removing initial/trailing two characters */
            for (i = 0; i < best_count; i++) {
                if (best_candidates[i]->key_len > 2) {
                    char *old_key = best_candidates[i]->key;
                    int old_len = best_candidates[i]->key_len;
                    int old_cost = best_candidates[i]->cost;
                    int delta;

                    /* Remove first 2 chars */
                    best_candidates[i]->key = xstrdup(old_key + 2);
                    best_candidates[i]->key_len = old_len - 2;
                    best_candidates[i]->cost = zstring_cost(alpha, best_candidates[i]->key, best_candidates[i]->key_len);
                    pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    latest_total_bytes = rescore_optimal_parse(&game_text_list,
                        best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                    delta = previous_total_bytes - latest_total_bytes;
                    if (delta > 0) {
                        previous_total_bytes = latest_total_bytes;
                        free(old_key);
                        old_key = best_candidates[i]->key;
                        old_len = best_candidates[i]->key_len;
                        old_cost = best_candidates[i]->cost;
                    } else {
                        free(best_candidates[i]->key);
                        best_candidates[i]->key = old_key;
                        best_candidates[i]->key_len = old_len;
                        best_candidates[i]->cost = old_cost;
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                    }

                    /* Remove last 2 chars */
                    if (best_candidates[i]->key_len > 2) {
                        old_key = best_candidates[i]->key;
                        old_len = best_candidates[i]->key_len;
                        old_cost = best_candidates[i]->cost;
                        best_candidates[i]->key = xstrndup(old_key, (size_t)(old_len - 2));
                        best_candidates[i]->key_len = old_len - 2;
                        best_candidates[i]->cost = zstring_cost(alpha, best_candidates[i]->key, best_candidates[i]->key_len);
                        pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                        latest_total_bytes = rescore_optimal_parse(&game_text_list,
                            best_candidates, best_count, 1, z_version, alpha, &routine_sizes);
                        delta = previous_total_bytes - latest_total_bytes;
                        if (delta > 0) {
                            previous_total_bytes = latest_total_bytes;
                            free(old_key);
                        } else {
                            free(best_candidates[i]->key);
                            best_candidates[i]->key = old_key;
                            best_candidates[i]->key_len = old_len;
                            best_candidates[i]->cost = old_cost;
                            pattern_update_occurrences(best_candidates[i], &game_text_list, 1);
                        }
                    }
                }
            }
        }

        latest_total_bytes = previous_total_bytes;
        total_savings = rescore_optimal_parse(&game_text_list,
            best_candidates, best_count, 0, z_version, alpha, &routine_sizes);

        {
            double elapsed = (double)(clock() - t_part) / CLOCKS_PER_SEC;
            if (!quiet_flag)
                fprintf(stderr, "%7.3f  Total saving %d z-chars, text size = %d bytes\n",
                        elapsed, total_savings, latest_total_bytes);
        }
    }

    /***************************************************************
     * Step 8: Output
     ***************************************************************/
    if (!only_refactor) {
        int total_savings_alpha = 0;
        double total_elapsed;

        if (alpha->custom) {
            int cost_default = 0, cost_custom = 0;
            size_t gi;
            for (gi = 0; gi < game_text_list.len; gi++) {
                cost_default += game_text_list.data[gi].cost_default_alpha;
                cost_custom += game_text_list.data[gi].cost_custom_alpha;
            }
            total_savings_alpha = cost_default - cost_custom;
            total_savings += total_savings_alpha;
        }

        total_elapsed = (double)(clock() - t_start) / CLOCKS_PER_SEC;
        if (!quiet_flag)
            fprintf(stderr, "\nTotal elapsed time: %7.3f s\n\n", total_elapsed);

        /* Final rescore */
        rescore_optimal_parse(&game_text_list, best_candidates, best_count,
                              0, z_version, alpha, &routine_sizes);
        total_savings = 0;
        {
            int max_a = best_count - 1;
            int ii;
            if (max_a >= number_of_abbrevs) max_a = number_of_abbrevs - 1;
            for (ii = 0; ii <= max_a; ii++)
                total_savings += best_candidates[ii]->savings;
        }
        if (!quiet_flag)
            fprintf(stderr, "Abbreviations would save %d z-chars total (~%d bytes)\n\n",
                    total_savings, (int)(total_savings * 2 / 3.0));

        /* Print statistics */
        if (!quiet_flag) {
            int total_bytes_val = 0;
            int total_wasted = 0;
            int total_bytes_abbrevs = 0;
            int pass;

            for (pass = 0; pass < number_of_abbrevs && pass < best_count; pass++)
                total_bytes_abbrevs += 2 * ((best_candidates[pass]->cost + 2) / 3);

            for (pass = 0; pass <= 1; pass++) {
                int wasted[12], bytes_arr[12];
                int total_bytes_pass = 0;
                int total_strings = 0;
                size_t gi;
                int ii;
                memset(wasted, 0, sizeof(wasted));
                memset(bytes_arr, 0, sizeof(bytes_arr));

                for (gi = 0; gi < game_text_list.len; gi++) {
                    struct game_text *line = &game_text_list.data[gi];
                    if (pass == 0 && line->packed_address) {
                        wasted[line->latest_rounding_cost]++;
                        bytes_arr[line->latest_rounding_cost] += line->latest_total_bytes;
                    }
                    if (pass == 1 && !line->packed_address) {
                        wasted[line->latest_rounding_cost]++;
                        bytes_arr[line->latest_rounding_cost] += line->latest_total_bytes;
                    }
                }

                for (ii = 0; ii < 12; ii++) total_strings += wasted[ii];
                if (pass == 0)
                    fprintf(stderr, "High memory strings (%d strings):\n", total_strings);
                else
                    fprintf(stderr, "Dynamic and static memory strings (%d strings):\n", total_strings);

                for (ii = 11; ii >= 0; ii--) {
                    total_bytes_val += bytes_arr[ii];
                    total_bytes_pass += bytes_arr[ii];
                    total_wasted += wasted[ii] * ii;
                    if (wasted[ii] > 0)
                        fprintf(stderr, "%6d strings with %2d empty z-chars, total = %8d, %7d bytes\n",
                                wasted[ii], ii, wasted[ii] * ii, bytes_arr[ii]);
                }
                fprintf(stderr, "                                                        -------\n");
                fprintf(stderr, "                                                        %7d bytes\n", total_bytes_pass);
            }
            fprintf(stderr, "                                                ===============\n");
            fprintf(stderr, "Total:                                       %9d, %7d bytes\n", total_wasted, total_bytes_val);
            fprintf(stderr, "\n");
            fprintf(stderr, "Storage size for the %2d abbreviations:                  %7d bytes\n", number_of_abbrevs, total_bytes_abbrevs);
            fprintf(stderr, "Storage size for the strings:                         + %7d bytes\n", total_bytes_val);
            fprintf(stderr, "                                                      =========\n");
            fprintf(stderr, "                                                        %7d bytes\n", total_bytes_val + total_bytes_abbrevs);
            fprintf(stderr, "\n");
        }

    }

    /* Verbose / refactoring output */
    if (verbose_flag || only_refactor) {
        size_t gi;
        int wasted;

        if (!only_refactor) {
            /* Part A: Print all strings with abbreviation markers */
            fprintf(stderr,
                    "High memory strings (abbreviations inside {},"
                    " ^ = linebreak and ~ = double-quote):\n");
            for (wasted = 11; wasted >= 0; wasted--) {
                for (gi = 0; gi < game_text_list.len; gi++) {
                    if (game_text_list.data[gi].packed_address &&
                        game_text_list.data[gi].latest_rounding_cost == wasted)
                        print_finished_text(stderr, &game_text_list.data[gi],
                                            best_candidates, best_count);
                }
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "Dynamic and static memory strings:\n");
            for (wasted = 2; wasted >= 0; wasted--) {
                for (gi = 0; gi < game_text_list.len; gi++) {
                    if (!game_text_list.data[gi].packed_address &&
                        game_text_list.data[gi].latest_rounding_cost == wasted)
                        print_finished_text(stderr, &game_text_list.data[gi],
                                            best_candidates, best_count);
                }
            }
            fprintf(stderr, "\n");
        }

        /* Part B: Long repeated strings / refactoring suggestions */
        print_refactoring(stderr, &max_heap_refactor, &game_text_list,
                          alpha, z_version);
    }

    if (!only_refactor) {
        /* Print final output */
        if (alpha->custom) {
            if (inform6_style) print_alphabet_i6(alpha);
            else print_alphabet(alpha);
        }

        if (inform6_style) {
            struct pattern_data **sorted = abbrev_sort(best_candidates, best_count, number_of_abbrevs, 0);
            print_abbreviations_i6(sorted, number_of_abbrevs, 0, number_of_abbrevs);
            {
                int ii;
                for (ii = 0; ii < number_of_abbrevs && ii < best_count; ii++)
                    pattern_data_free(sorted[ii]);
            }
            free(sorted);
        } else {
            print_abbreviations(best_candidates, best_count, game_filename, 0, number_of_abbrevs);
        }
    }

cleanup:
    free(game_filename);
    free(gsa_string);
    free(sa);
    free(lcp);
    free(best_candidates);
    gt_array_free(&game_text_list);
    /* Note: pattern_dict entries are freed with the table */
    {
        size_t bi;
        for (bi = 0; bi < pattern_dict.num_buckets; bi++) {
            struct hash_entry *e = pattern_dict.buckets[bi];
            while (e) {
                if (e->value) {
                    pattern_data_free(e->value);
                    free(e->value);
                }
                e = e->next;
            }
        }
    }
    hash_free(&pattern_dict);
    heap_free(&max_heap);
    heap_free(&max_heap_refactor);
    int_array_free(&routine_sizes);
}
