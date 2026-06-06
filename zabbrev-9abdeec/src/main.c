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
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "zabbrev.h"

static void
print_version(void)
{
    printf("zabbrev %s\n", PACKAGE_VERSION);
    printf("Copyright (C) 2021-2026 Henrik \xc3\x85sman\n");
    printf("Copyright (C) 2026 Jason Self <j@jxself.org>\n");
    printf("License GPLv3+: GNU GPL version 3 or later"
           " <https://gnu.org/licenses/gpl.html>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static void
print_help(void)
{
    printf("Usage: zabbrev [OPTION]... [PATH-TO-GAME]\n");
    printf("Compute highly optimized abbreviations for Z-machine text compression.\n");
    printf("\n");
    printf("  -a, -A, --alphabet     Create a tailor-made alphabet for this game\n");
    printf("      --a0 STRING        Define 26 characters for alphabet A0\n");
    printf("      --a1 STRING        Define 26 characters for alphabet A1\n");
    printf("      --a2 STRING        Define 23 characters for alphabet A2\n");
    printf("  -b, --throwback        Throw back low-scorers onto heap\n");
    printf("      --c0               Text charset: ASCII\n");
    printf("      --cu               Text charset: UTF-8\n");
    printf("      --c1               Text charset: ISO 8859-1\n");
    printf("      --debug            Print debug information\n");
    printf("  -h, --help             Print this help and exit\n");
    printf("  -i, --inform           Generate Inform6-style output\n");
    printf("      --infodump FILE    Use infodump-extracted text\n");
    printf("  -n, --number N         Number of abbreviations (default 96)\n");
    printf("  -o, --output FORMAT    Output format: input/inform/zap\n");
    printf("      --onlyrefactor     Only print duplicate-string info\n");
    printf("  -q, --quiet, --silent  Do not print the startup banner\n");
    printf("      --r3               Force rounding to 3\n");
    printf("      --txd FILE         Use txd-extracted text\n");
    printf("  -v, --verbose          Print extra information\n");
    printf("      --version          Print version and exit\n");
    printf("  -x LEVEL               Compression level (0-3, default 2)\n");
    printf("\n");
    printf("Compression levels:\n");
    printf("  -x0              Fastest. Pick by highest score with optimal parse.\n");
    printf("  -x1              Fast. x0 + add/remove initial/trailing characters.\n");
    printf("  -x2 [n]          Normal (default). x1 + test n discarded variants.\n");
    printf("  -x3 [n1] [n2]    Maximum. x2 + deep replacement testing.\n");
    printf("\n");
    printf("Z-machine version options: -v1 through -v8\n");
    printf("  v1-v3: Round to 3 for high strings\n");
    printf("  v4-v7: Round to 6 for high strings\n");
    printf("  v8:    Round to 12 for high strings\n");
    printf("\n");
    printf("Report bugs to: <%s>\n", PACKAGE_BUGREPORT);
    printf("zabbrev home page: <%s>\n", PACKAGE_URL);
}

static int
dir_exists_check(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static int
file_exists_check(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

int
main(int argc, char **argv)
{
    int force_rounding_to_3 = 0;
    int print_debug = 0;
    int throw_back_lowscorers = 0;
    int inform6_style = 0;
    int verbose_flag = 0;
    int quiet_flag = 0;
    int only_refactor = 0;
    int compression_level = 2;
    int number_of_abbrevs = NUMBER_OF_ABBREVIATIONS;
    int number_of_passes = NUMBER_OF_PASSES_DEFAULT;
    int number_of_deep_passes = NUMBER_OF_DEEP_PASSES_DEFAULT;
    int z_version = 0;
    int try_auto_detect = 0;
    int output_format = 0;
    int text_encoding = ENC_NONE;
    struct alphabet_state alpha;
    char *game_directory = NULL;
    char *infodump_filename = NULL;
    char *txd_filename = NULL;
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
        {"alphabet",    no_argument,       0, 'a'},
        {"a0",          required_argument, 0, 1001},
        {"a1",          required_argument, 0, 1002},
        {"a2",          required_argument, 0, 1003},
        {"throwback",   no_argument,       0, 'b'},
        {"c0",          no_argument,       0, 1004},
        {"cu",          no_argument,       0, 1005},
        {"c1",          no_argument,       0, 1006},
        {"debug",       no_argument,       0, 1007},
        {"help",        no_argument,       0, 'h'},
        {"inform",      no_argument,       0, 'i'},
        {"infodump",    required_argument, 0, 1008},
        {"number",      required_argument, 0, 'n'},
        {"output",      required_argument, 0, 'o'},
        {"onlyrefactor",no_argument,       0, 1009},
        {"quiet",       no_argument,       0, 'q'},
        {"r3",          no_argument,       0, 1010},
        {"silent",      no_argument,       0, 'q'},
        {"txd",         required_argument, 0, 1011},
        {"verbose",     no_argument,       0, 'v'},
        {"version",     no_argument,       0, 1012},
        {0, 0, 0, 0}
    };

    alphabet_init(&alpha);
    game_directory = xstrdup(".");
    infodump_filename = xstrdup("");
    txd_filename = xstrdup("");

    /* We need custom parsing for -x0, -x1, -x2, -x3, -v1..-v8 since
       getopt_long can't handle them easily. Pre-scan argv for these. */
    {
        int i;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-x0") == 0) {
                compression_level = 0;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-x1") == 0) {
                compression_level = 1;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-x2") == 0) {
                compression_level = 2;
                argv[i] = (char *)"--processed";
                if (i + 1 < argc) {
                    char *endp;
                    long val = strtol(argv[i + 1], &endp, 10);
                    if (*endp == '\0' && val > 0) {
                        number_of_passes = (int)val;
                        argv[++i] = (char *)"--processed";
                    }
                }
            } else if (strcmp(argv[i], "-x3") == 0) {
                compression_level = 3;
                argv[i] = (char *)"--processed";
                if (i + 1 < argc) {
                    char *endp;
                    long val = strtol(argv[i + 1], &endp, 10);
                    if (*endp == '\0' && val > 0) {
                        number_of_passes = (int)val;
                        argv[++i] = (char *)"--processed";
                    }
                }
                if (i + 1 < argc) {
                    char *endp;
                    long val = strtol(argv[i + 1], &endp, 10);
                    if (*endp == '\0' && val > 0) {
                        number_of_deep_passes = (int)val;
                        argv[++i] = (char *)"--processed";
                    }
                }
                if (number_of_passes < number_of_deep_passes)
                    number_of_passes = number_of_deep_passes;
            } else if (strcmp(argv[i], "-v1") == 0 ||
                       strcmp(argv[i], "-v2") == 0 ||
                       strcmp(argv[i], "-v3") == 0) {
                z_version = 3;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-v4") == 0 ||
                       strcmp(argv[i], "-v5") == 0 ||
                       strcmp(argv[i], "-v6") == 0 ||
                       strcmp(argv[i], "-v7") == 0) {
                z_version = 5;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-v8") == 0) {
                z_version = 8;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-r3") == 0) {
                force_rounding_to_3 = 1;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-c0") == 0) {
                text_encoding = ENC_ASCII;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-cu") == 0) {
                text_encoding = ENC_UTF8;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "-c1") == 0) {
                text_encoding = ENC_LATIN1;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--infodump") == 0 && i + 1 < argc) {
                free(infodump_filename);
                infodump_filename = xstrdup(argv[i + 1]);
                argv[i] = (char *)"--processed";
                argv[++i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--txd") == 0 && i + 1 < argc) {
                free(txd_filename);
                txd_filename = xstrdup(argv[i + 1]);
                argv[i] = (char *)"--processed";
                argv[++i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--onlyrefactor") == 0) {
                only_refactor = 1;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--debug") == 0) {
                print_debug = 1;
                argv[i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--a0") == 0 && i + 1 < argc) {
                if (strlen(argv[i + 1]) == 26) {
                    strncpy(alpha.a0, argv[i + 1], 26);
                    alpha.a0[26] = '\0';
                } else {
                    fprintf(stderr, "WARNING: Can't use defined A0 (needs 26 characters). Using default instead.\n");
                }
                argv[i] = (char *)"--processed";
                argv[++i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--a1") == 0 && i + 1 < argc) {
                if (strlen(argv[i + 1]) == 26) {
                    strncpy(alpha.a1, argv[i + 1], 26);
                    alpha.a1[26] = '\0';
                } else {
                    fprintf(stderr, "WARNING: Can't use defined A1 (needs 26 characters). Using default instead.\n");
                }
                argv[i] = (char *)"--processed";
                argv[++i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--a2") == 0 && i + 1 < argc) {
                if (strlen(argv[i + 1]) == 23) {
                    strncpy(alpha.a2, argv[i + 1], 23);
                    alpha.a2[23] = '\0';
                } else {
                    fprintf(stderr, "WARNING: Can't use defined A2 (needs 23 characters). Using default instead.\n");
                }
                argv[i] = (char *)"--processed";
                argv[++i] = (char *)"--processed";
            } else if (strcmp(argv[i], "--processed") == 0) {
                /* already handled */
            }
        }
    }

    /* Reset optind for getopt_long */
    optind = 1;
    opterr = 0; /* Suppress getopt errors for unknown options */

    while ((opt = getopt_long(argc, argv, "Aabn:o:ihqv", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'A':
            alpha.custom = 1;
            break;
        case 'a':
            alpha.custom = 1;
            break;
        case 'b':
            throw_back_lowscorers = 1;
            break;
        case 'n':
            number_of_abbrevs = atoi(optarg);
            if (number_of_abbrevs < 1) number_of_abbrevs = NUMBER_OF_ABBREVIATIONS;
            break;
        case 'o':
            if (strcmp(optarg, "0") == 0 || strcasecmp(optarg, "input") == 0)
                output_format = 0;
            else if (strcmp(optarg, "1") == 0 || strcasecmp(optarg, "inform") == 0)
                output_format = 1;
            else if (strcmp(optarg, "2") == 0 || strcasecmp(optarg, "zap") == 0)
                output_format = 2;
            break;
        case 'i':
            inform6_style = 1;
            break;
        case 'h':
            print_help();
            free(game_directory);
            free(infodump_filename);
            free(txd_filename);
            return EXIT_SUCCESS;
        case 'q':
            quiet_flag = 1;
            break;
        case 'v':
            verbose_flag = 1;
            break;
        case 1004: /* --c0 */
            text_encoding = ENC_ASCII;
            break;
        case 1005: /* --cu */
            text_encoding = ENC_UTF8;
            break;
        case 1006: /* --c1 */
            text_encoding = ENC_LATIN1;
            break;
        case 1007: /* --debug */
            print_debug = 1;
            break;
        case 1009: /* --onlyrefactor */
            only_refactor = 1;
            break;
        case 1010: /* --r3 */
            force_rounding_to_3 = 1;
            break;
        case 1012: /* --version */
            print_version();
            free(game_directory);
            free(infodump_filename);
            free(txd_filename);
            return EXIT_SUCCESS;
        case '?':
            /* Unknown option - might be already processed or a positional arg */
            break;
        default:
            break;
        }
    }

    /* Remaining non-option arguments: game directory */
    while (optind < argc) {
        if (strcmp(argv[optind], "--processed") != 0 &&
            dir_exists_check(argv[optind])) {
            free(game_directory);
            game_directory = xstrdup(argv[optind]);
        }
        optind++;
    }

    /* If no args and no files to work with, print help */
    if (argc == 1) {
        char *gt_path = (char *)xmalloc(strlen(game_directory) + 20);
        int has_zap = 0;
        DIR *d;

        sprintf(gt_path, "%s/gametext.txt", game_directory);
        if (!file_exists_check(gt_path)) {
            d = opendir(game_directory);
            if (d) {
                struct dirent *ent;
                while ((ent = readdir(d)) != NULL) {
                    const char *dot = strrchr(ent->d_name, '.');
                    if (dot && strcasecmp(dot, ".zap") == 0) {
                        has_zap = 1;
                        break;
                    }
                }
                closedir(d);
            }
            if (!has_zap) {
                print_help();
                free(gt_path);
                free(game_directory);
                free(infodump_filename);
                free(txd_filename);
                return EXIT_SUCCESS;
            }
        }
        free(gt_path);
    }

    if (force_rounding_to_3) z_version = 3;
    if (z_version == 0) {
        z_version = 3;
        try_auto_detect = 1;
    }

    /* Auto-detect Inform */
    if (!inform6_style) {
        char *gt_path = (char *)xmalloc(strlen(game_directory) + 20);
        sprintf(gt_path, "%s/gametext.txt", game_directory);
        if (file_exists_check(gt_path))
            inform6_style = 1;
        free(gt_path);
    }

    search_for_abbreviations(game_directory, inform6_style,
                             throw_back_lowscorers, print_debug,
                             verbose_flag, quiet_flag, only_refactor,
                             compression_level, number_of_abbrevs,
                             number_of_passes, number_of_deep_passes,
                             z_version, try_auto_detect,
                             output_format, text_encoding,
                             &alpha, infodump_filename, txd_filename);

    free(game_directory);
    free(infodump_filename);
    free(txd_filename);
    return EXIT_SUCCESS;
}
