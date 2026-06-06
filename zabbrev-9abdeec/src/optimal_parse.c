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
#include "zabbrev.h"

/*
 * RescoreOptimalParse - Parse strings using Wagner's optimal parse.
 *
 * Almost all execution time is spent here (~96.5%).
 * Optimization for speed is of ESSENCE!
 */
int
rescore_optimal_parse(struct game_text_array *game_text,
                      struct pattern_data **abbreviations,
                      int abbrev_count,
                      int return_total_bytes,
                      int zversion,
                      const struct alphabet_state *alpha,
                      const struct int_array *routine_sizes)
{
    int total_bytes = 0;
    int total_savings = 0;
    int total_padding = 0;
    int game_text_count = (int)game_text->len;
    int line, a;

    /* Clear frequency from abbrevs and init occurrences */
    for (a = 0; a < abbrev_count; a++) {
        abbreviations[a]->frequency = 0;
        abbreviations[a]->key_len = (int)strlen(abbreviations[a]->key);
        pattern_update_occurrences(abbreviations[a], game_text, 0);
    }

    /* Iterate over each string */
    for (line = 0; line < game_text_count; line++) {
        struct game_text *gt = &game_text->data[line];
        int text_length = gt->text_length;
        int *minimal_cost = gt->minimal_cost;
        int *picked = gt->picked_abbrevs;
        int index;

        /* Build possible abbreviations array for this line */
        /* Use a flat array of int_arrays, one per position */
        struct int_array *possible = (struct int_array *)xcalloc(
            (size_t)text_length, sizeof(struct int_array));

        for (a = 0; a < abbrev_count; a++) {
            struct int_array *occ = &abbreviations[a]->occurrences[line];
            if (occ->len > 0) {
                size_t p;
                for (p = 0; p < occ->len; p++) {
                    int pos = occ->data[p];
                    if (pos < text_length)
                        int_array_push(&possible[pos], a);
                }
            }
        }

        /* Initialize picked_abbrevs to -1 */
        memset(picked, -1, (size_t)(text_length + 1) * sizeof(int));
        minimal_cost[text_length] = 0;

        /* Reverse iteration - optimal parse */
        for (index = text_length - 1; index >= 0; index--) {
            int char_cost = zchar_cost(alpha, (unsigned char)gt->text[index]);
            minimal_cost[index] = minimal_cost[index + 1] + char_cost;

            if (possible[index].len > 0) {
                size_t pi;
                for (pi = 0; pi < possible[index].len; pi++) {
                    int abbrev_no = possible[index].data[pi];
                    int abbrev_len = abbreviations[abbrev_no]->key_len;
                    int cost_with_pattern = ABBREVIATION_REF_COST +
                                            minimal_cost[index + abbrev_len];

                    if (cost_with_pattern < minimal_cost[index]) {
                        picked[index] = abbrev_no;
                        minimal_cost[index] = cost_with_pattern;
                    }

                    /* Tie-breaking: prefer longer/higher-cost abbreviations */
                    if (picked[index] != -1 &&
                        cost_with_pattern == minimal_cost[index] &&
                        abbreviations[abbrev_no]->cost >=
                        abbreviations[picked[index]]->cost) {
                        picked[index] = abbrev_no;
                        minimal_cost[index] = cost_with_pattern;
                    }
                }
            }
        }

        /* Free possible arrays */
        {
            int pi;
            for (pi = 0; pi < text_length; pi++)
                int_array_free(&possible[pi]);
            free(possible);
        }

        /* Update frequencies */
        for (index = 0; index < text_length; ) {
            if (picked[index] > -1) {
                abbreviations[picked[index]]->frequency++;
                index += abbreviations[picked[index]]->key_len;
            } else {
                index++;
            }
        }

        /* Aggregate rounding */
        {
            int rn = gt->rounding_number;
            gt->latest_rounding_cost = (rn - (minimal_cost[0] % rn)) % rn;
            gt->latest_minimal_cost = minimal_cost[0];
            gt->latest_total_bytes = ((gt->latest_minimal_cost +
                                       gt->latest_rounding_cost) * 2) / 3;
            total_bytes += gt->latest_total_bytes;
        }
    }

    /* Summarize savings and bytes for picked abbreviations */
    for (a = 0; a < abbrev_count; a++) {
        abbreviations[a]->savings = pattern_score(abbreviations[a]);
        total_savings += abbreviations[a]->savings;
        total_bytes += 2 * ((abbreviations[a]->cost + 2) / 3);
    }

    /* Add padding cost from z-code routines, if available */
    if (return_total_bytes && routine_sizes) {
        int latest_start = 0;
        int rs_count = (int)routine_sizes->len;
        int ri;
        for (ri = 0; ri < rs_count; ri++) {
            int rsize = routine_sizes->data[ri];
            int j;
            for (j = latest_start; j < game_text_count; j++) {
                if (game_text->data[j].routine_id > ri) {
                    latest_start = j;
                    break;
                }
                if (game_text->data[j].routine_id == ri)
                    rsize += game_text->data[j].latest_total_bytes;
            }
            if (zversion < 4 && (rsize & 1) == 1)
                total_padding += 1;
            if (zversion > 3 && zversion < 8)
                total_padding += (4 - (rsize % 4)) % 4;
            if (zversion == 8)
                total_padding += (8 - (rsize % 8)) % 8;
        }
    }

    if (return_total_bytes)
        return total_bytes + total_padding;
    else
        return total_savings;
}
