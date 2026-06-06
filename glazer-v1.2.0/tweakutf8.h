/*
MIT License

Copyright (c) 2024 Andrew Apted

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __TWEAKUTF8_H__
#define __TWEAKUTF8_H__

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// current version
#define TWEAK_VERSION_STR   "1.0.0"
#define TWEAK_VERSION_NUM   (1*100 + 0*10 + 0)

// a Unicode (UCS-4) code-point
typedef unsigned int  tw_char_t;

// a value representing NUL (string terminator)
#define TW_NUL  0

// a value representing EOF (End Of File)
#define TW_EOF  0x7FFFFFFF

// a test for special keys and characters
#define TW_IS_SPECIAL(ch)  (((tw_char_t)(ch) & 0xFF000000u) != 0)

/* encode a single unicode char into UTF-8, returning the length of
   the UTF-8 sequence (always between 1 and 4).
*/
int tweak_utf8_encode_char (char * out, tw_char_t ch);

/* encode the given "wide string" as a UTF-8 string.
   the input must be terminated by a NUL character, and the output
   will always be terminated with a NUL, even if string is truncated.
   invalid code-points will encode the unknown symbol char: U+FFFD.

   returns -1 if the string was truncated (not enough room in the
   output buffer), otherwise the length of the output string
   (excluding the trailing NUL) is returned.
*/
int tweak_utf8_encode_str (char * out, int max_out, const tw_char_t * in);

/* decode a single unicode char from UTF-8, returning the length of
   the UTF-8 sequence (always between 1 and 4).  invalid sequences
   produce the unknown symbol character: U+FFFD.
*/
int tweak_utf8_decode_char (tw_char_t * out, const char * s);

/* convert a NUL-terminated string in UTF-8 encoding to a sequence of
   of unicode code points (32-bits each).  the output is terminated by
   a `NUL` character.  invalid sequences produce the unknown symbol
   character: U+FFFD.

   returns -1 if the string was truncated (not enough room in the
   output buffer), otherwise the length of the output string
   (excluding the trailing NUL) is returned.
*/
int tweak_utf8_decode_str (tw_char_t * out, int max_out, const char * s);

/* convert a NUL-terminated UTF-8 string to a UTF-16 / UTF-32 string.
   the output is terminated by a `NUL` character.  invalid sequences
   produce the unknown symbol character: U+FFFD.  

   returns -1 if the string was truncated (not enough room in the
   output buffer), otherwise the length of the output string
   (excluding the trailing NUL) is returned.
*/
int tweak_utf8_to_utf16 (unsigned short * out, int max_out, const char * s);
int tweak_utf8_to_utf32 (tw_char_t      * out, int max_out, const char * s);

/* convert a NUL-terminated UTF-16 / UTF-32 string to a UTF-8 string.
   the output is terminated by a `NUL` character.  in UTF-16, an invalid
   surrogate pair produces the unknown symbol: U+FFFD.  in UTF-32, any
   surrogate pair becomes U+FFFD.

   returns -1 if the string was truncated (not enough room in the
   output buffer), otherwise the length of the output string
   (excluding the trailing NUL) is returned.
*/
int tweak_utf8_from_utf16 (char * out, int max_out, const unsigned short * in);
int tweak_utf8_from_utf32 (char * out, int max_out, const tw_char_t      * in);

/* convert a single unicode char to UTF-8 and write the byte(s)
   into the given file.  returns the number of bytes written (1..4),
   or a negative value for a write error.
*/
int tweak_utf8_fputc (tw_char_t ch, FILE * fp);

/* read a single unicode character (encoded as UTF-8) from the given
   file.  returns TW_EOF on end of file (or a read error), and U+FFFD
   when an invalid UTF-8 sequence is encountered.
*/
tw_char_t tweak_utf8_fgetc (FILE * fp);

/* determine how many bytes in a UTF-8 encoded unicode char based on
   the starting byte.  result is always between 1 and 4.
*/
int tweak_utf8_char_length (char first);

/* determine the length of the NUL-terminated wide string in s.
*/
int tweak_wide_strlen (const tw_char_t * s);

/* determine the width (number of cells on a terminal) for the
   given character, as follows:
   -  0 for control codes and most composing characters
   -  2 for wide characters (e.g. Chinese/Japanese/Korean ideographs)
   -  1 for everything else (e.g. ASCII, Latin, Cyrillic, Greek)
*/
int tweak_char_width (tw_char_t ch);

/* returns 1 if the character is a composing character, 0 otherwise.
*/
int tweak_char_is_compose (tw_char_t ch);

enum
{
	TW_BOM_NONE = 0,
	TW_BOM_UTF_8,
	TW_BOM_UTF_16,
	TW_BOM_UTF_16_BE
};

/* helper to detect a Byte Order Mark (BOM) at beginning of a file.
   there should be least four bytes of data.  returns one of the
   `TW_BOM_XX` values defined above.  note that UTF-32 versions
   are not detected, since I am hoping such files *do not exist*.
*/
int tweak_detect_bom (unsigned char * data, int length);

#ifdef __cplusplus
}
#endif

#endif /* __TWEAKUTF8_H__ */

//----------------------------------------------------------------------------

#ifdef TWEAK_IMPL

#if defined(_WIN32) || defined(_WIN64)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif

  #include <windows.h>
  #ifndef __WIN32__
  #define __WIN32__
  #endif
#endif

struct TweakCharPair
{
	tw_char_t  low, high;
};

#define NUM_ZERO_WIDTHS  52
static const tw_char_t  tw_table_zero_width[NUM_ZERO_WIDTHS] =
{
	0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x061C,
	0x06DD, 0x070F, 0x0890, 0x0891, 0x08E2, 0x180E, 0x200B, 0x200C,
	0x200D, 0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E,
	0x2060, 0x2061, 0x2062, 0x2063, 0x2064, 0x2066, 0x2067, 0x2068,
	0x2069, 0x206A, 0x206B, 0x206C, 0x206D, 0x206E, 0x206F,
	0xFEFF, 0xFFF9, 0xFFFA, 0xFFFB, 0x110BD, 0x110CD,
	0x1D173, 0x1D174, 0x1D175, 0x1D176,
	0x1D177, 0x1D178, 0x1D179, 0x1D17A
};

#define NUM_COMPOSE_SPACINGS  452
static const tw_char_t  tw_table_compose_spacing[NUM_COMPOSE_SPACINGS] =
{
	0x0903, 0x093B, 0x093E, 0x093F, 0x0940, 0x0949, 0x094A, 0x094B,
	0x094C, 0x094E, 0x094F, 0x0982, 0x0983, 0x09BE, 0x09BF, 0x09C0,
	0x09C7, 0x09C8, 0x09CB, 0x09CC, 0x09D7, 0x0A03, 0x0A3E, 0x0A3F,
	0x0A40, 0x0A83, 0x0ABE, 0x0ABF, 0x0AC0, 0x0AC9, 0x0ACB, 0x0ACC,
	0x0B02, 0x0B03, 0x0B3E, 0x0B40, 0x0B47, 0x0B48, 0x0B4B, 0x0B4C,
	0x0B57, 0x0BBE, 0x0BBF, 0x0BC1, 0x0BC2, 0x0BC6, 0x0BC7, 0x0BC8,
	0x0BCA, 0x0BCB, 0x0BCC, 0x0BD7, 0x0C01, 0x0C02, 0x0C03, 0x0C41,
	0x0C42, 0x0C43, 0x0C44, 0x0C82, 0x0C83, 0x0CBE, 0x0CC0, 0x0CC1,
	0x0CC2, 0x0CC3, 0x0CC4, 0x0CC7, 0x0CC8, 0x0CCA, 0x0CCB, 0x0CD5,
	0x0CD6, 0x0CF3, 0x0D02, 0x0D03, 0x0D3E, 0x0D3F, 0x0D40, 0x0D46,
	0x0D47, 0x0D48, 0x0D4A, 0x0D4B, 0x0D4C, 0x0D57, 0x0D82, 0x0D83,
	0x0DCF, 0x0DD0, 0x0DD1, 0x0DD8, 0x0DD9, 0x0DDA, 0x0DDB, 0x0DDC,
	0x0DDD, 0x0DDE, 0x0DDF, 0x0DF2, 0x0DF3, 0x0F3E, 0x0F3F, 0x0F7F,
	0x102B, 0x102C, 0x1031, 0x1038, 0x103B, 0x103C, 0x1056, 0x1057,
	0x1062, 0x1063, 0x1064, 0x1067, 0x1068, 0x1069, 0x106A, 0x106B,
	0x106C, 0x106D, 0x1083, 0x1084, 0x1087, 0x1088, 0x1089, 0x108A,
	0x108B, 0x108C, 0x108F, 0x109A, 0x109B, 0x109C, 0x1715, 0x1734,
	0x17B6, 0x17BE, 0x17BF, 0x17C0, 0x17C1, 0x17C2, 0x17C3, 0x17C4,
	0x17C5, 0x17C7, 0x17C8, 0x1923, 0x1924, 0x1925, 0x1926, 0x1929,
	0x192A, 0x192B, 0x1930, 0x1931, 0x1933, 0x1934, 0x1935, 0x1936,
	0x1937, 0x1938, 0x1A19, 0x1A1A, 0x1A55, 0x1A57, 0x1A61, 0x1A63,
	0x1A64, 0x1A6D, 0x1A6E, 0x1A6F, 0x1A70, 0x1A71, 0x1A72, 0x1B04,
	0x1B35, 0x1B3B, 0x1B3D, 0x1B3E, 0x1B3F, 0x1B40, 0x1B41, 0x1B43,
	0x1B44, 0x1B82, 0x1BA1, 0x1BA6, 0x1BA7, 0x1BAA, 0x1BE7, 0x1BEA,
	0x1BEB, 0x1BEC, 0x1BEE, 0x1BF2, 0x1BF3, 0x1C24, 0x1C25, 0x1C26,
	0x1C27, 0x1C28, 0x1C29, 0x1C2A, 0x1C2B, 0x1C34, 0x1C35, 0x1CE1,
	0x1CF7, 0x302E, 0x302F, 0xA823, 0xA824, 0xA827, 0xA880, 0xA881,
	0xA8B4, 0xA8B5, 0xA8B6, 0xA8B7, 0xA8B8, 0xA8B9, 0xA8BA, 0xA8BB,
	0xA8BC, 0xA8BD, 0xA8BE, 0xA8BF, 0xA8C0, 0xA8C1, 0xA8C2, 0xA8C3,
	0xA952, 0xA953, 0xA983, 0xA9B4, 0xA9B5, 0xA9BA, 0xA9BB, 0xA9BE,
	0xA9BF, 0xA9C0, 0xAA2F, 0xAA30, 0xAA33, 0xAA34, 0xAA4D, 0xAA7B,
	0xAA7D, 0xAAEB, 0xAAEE, 0xAAEF, 0xAAF5, 0xABE3, 0xABE4, 0xABE6,
	0xABE7, 0xABE9, 0xABEA, 0xABEC,

	0x11000, 0x11002, 0x11082, 0x110B0,
	0x110B1, 0x110B2, 0x110B7, 0x110B8, 0x1112C, 0x11145, 0x11146, 0x11182,
	0x111B3, 0x111B4, 0x111B5, 0x111BF, 0x111C0, 0x111CE, 0x1122C, 0x1122D,
	0x1122E, 0x11232, 0x11233, 0x11235, 0x112E0, 0x112E1, 0x112E2, 0x11302,
	0x11303, 0x1133E, 0x1133F, 0x11341, 0x11342, 0x11343, 0x11344, 0x11347,
	0x11348, 0x1134B, 0x1134C, 0x1134D, 0x11357, 0x11362, 0x11363, 0x11435,
	0x11436, 0x11437, 0x11440, 0x11441, 0x11445, 0x114B0, 0x114B1, 0x114B2,
	0x114B9, 0x114BB, 0x114BC, 0x114BD, 0x114BE, 0x114C1, 0x115AF, 0x115B0,
	0x115B1, 0x115B8, 0x115B9, 0x115BA, 0x115BB, 0x115BE, 0x11630, 0x11631,
	0x11632, 0x1163B, 0x1163C, 0x1163E, 0x116AC, 0x116AE, 0x116AF, 0x116B6,
	0x11720, 0x11721, 0x11726, 0x1182C, 0x1182D, 0x1182E, 0x11838, 0x11930,
	0x11931, 0x11932, 0x11933, 0x11934, 0x11935, 0x11937, 0x11938, 0x1193D,
	0x11940, 0x11942, 0x119D1, 0x119D2, 0x119D3, 0x119DC, 0x119DD, 0x119DE,
	0x119DF, 0x119E4, 0x11A39, 0x11A57, 0x11A58, 0x11A97, 0x11C2F, 0x11C3E,
	0x11CA9, 0x11CB1, 0x11CB4, 0x11D8A, 0x11D8B, 0x11D8C, 0x11D8D, 0x11D8E,
	0x11D93, 0x11D94, 0x11D96, 0x11EF5, 0x11EF6, 0x11F03, 0x11F34, 0x11F35,
	0x11F3E, 0x11F3F, 0x11F41, 0x16F51, 0x16F52, 0x16F53, 0x16F54, 0x16F55,
	0x16F56, 0x16F57, 0x16F58, 0x16F59, 0x16F5A, 0x16F5B, 0x16F5C, 0x16F5D,
	0x16F5E, 0x16F5F, 0x16F60, 0x16F61, 0x16F62, 0x16F63, 0x16F64, 0x16F65,
	0x16F66, 0x16F67, 0x16F68, 0x16F69, 0x16F6A, 0x16F6B, 0x16F6C, 0x16F6D,
	0x16F6E, 0x16F6F, 0x16F70, 0x16F71, 0x16F72, 0x16F73, 0x16F74, 0x16F75,
	0x16F76, 0x16F77, 0x16F78, 0x16F79, 0x16F7A, 0x16F7B, 0x16F7C, 0x16F7D,
	0x16F7E, 0x16F7F, 0x16F80, 0x16F81, 0x16F82, 0x16F83, 0x16F84, 0x16F85,
	0x16F86, 0x16F87, 0x16FF0, 0x16FF1, 0x1D165, 0x1D166, 0x1D16D, 0x1D16E,
	0x1D16F, 0x1D170, 0x1D171, 0x1D172
};

#define NUM_COMPOSING_RANGES  235
static const struct TweakCharPair  tw_table_composing[NUM_COMPOSING_RANGES] =
{
	{ 0x0300, 0x036F }, { 0x0483, 0x0489 }, { 0x0591, 0x05BD }, { 0x05BF, 0x05BF },
	{ 0x05C1, 0x05C2 }, { 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0610, 0x061A },
	{ 0x064B, 0x065F }, { 0x0670, 0x0670 }, { 0x06D6, 0x06DC }, { 0x06DF, 0x06E4 },
	{ 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
	{ 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x07FD, 0x07FD }, { 0x0816, 0x0819 },
	{ 0x081B, 0x0823 }, { 0x0825, 0x0827 }, { 0x0829, 0x082D }, { 0x0859, 0x085B },
	{ 0x0898, 0x089F }, { 0x08CA, 0x08E1 }, { 0x08E3, 0x0902 }, { 0x093A, 0x093C },
	{ 0x0941, 0x094D }, { 0x0951, 0x0957 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
	{ 0x09BC, 0x09BC }, { 0x09C1, 0x09CD }, { 0x09E2, 0x09E3 }, { 0x09FE, 0x0A02 },
	{ 0x0A3C, 0x0A51 }, { 0x0A70, 0x0A71 }, { 0x0A75, 0x0A75 }, { 0x0A81, 0x0A82 },
	{ 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0ACD }, { 0x0AE2, 0x0AE3 }, { 0x0AFA, 0x0B01 },
	{ 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B56 }, { 0x0B62, 0x0B63 }, { 0x0B82, 0x0B82 },
	{ 0x0BC0, 0x0BCD }, { 0x0C00, 0x0C04 }, { 0x0C3C, 0x0C3C }, { 0x0C3E, 0x0C56 },
	{ 0x0C62, 0x0C63 }, { 0x0C81, 0x0C81 }, { 0x0CBC, 0x0CBC }, { 0x0CBF, 0x0CCD },
	{ 0x0CE2, 0x0CE3 }, { 0x0D00, 0x0D01 }, { 0x0D3B, 0x0D3C }, { 0x0D41, 0x0D4D },
	{ 0x0D62, 0x0D63 }, { 0x0D81, 0x0D81 }, { 0x0DCA, 0x0DD6 }, { 0x0E31, 0x0E31 },
	{ 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EBC },
	{ 0x0EC8, 0x0ECE }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 },
	{ 0x0F39, 0x0F39 }, { 0x0F71, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F8D, 0x0FBC },
	{ 0x0FC6, 0x0FC6 }, { 0x102D, 0x103E }, { 0x1058, 0x1059 }, { 0x105E, 0x1060 },
	{ 0x1071, 0x1074 }, { 0x1082, 0x108D }, { 0x109D, 0x109D }, { 0x135D, 0x135F },
	{ 0x1712, 0x1714 }, { 0x1732, 0x1733 }, { 0x1752, 0x1753 }, { 0x1772, 0x1773 },
	{ 0x17B4, 0x17D3 }, { 0x17DD, 0x17DD }, { 0x180B, 0x180D }, { 0x180F, 0x180F },
	{ 0x1885, 0x1886 }, { 0x18A9, 0x18A9 }, { 0x1920, 0x193B }, { 0x1A17, 0x1A1B },
	{ 0x1A56, 0x1A7F }, { 0x1AB0, 0x1B03 }, { 0x1B34, 0x1B42 }, { 0x1B6B, 0x1B73 },
	{ 0x1B80, 0x1B81 }, { 0x1BA2, 0x1BAD }, { 0x1BE6, 0x1BF1 }, { 0x1C2C, 0x1C37 },
	{ 0x1CD0, 0x1CD2 }, { 0x1CD4, 0x1CE8 }, { 0x1CED, 0x1CED }, { 0x1CF4, 0x1CF4 },
	{ 0x1CF8, 0x1CF9 }, { 0x1DC0, 0x1DFF }, { 0x20D0, 0x20F0 }, { 0x2CEF, 0x2CF1 },
	{ 0x2D7F, 0x2D7F }, { 0x2DE0, 0x2DFF }, { 0x302A, 0x302D }, { 0x3099, 0x309A },
	{ 0xA66F, 0xA672 }, { 0xA674, 0xA67D }, { 0xA69E, 0xA69F }, { 0xA6F0, 0xA6F1 },
	{ 0xA802, 0xA802 }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B }, { 0xA825, 0xA826 },
	{ 0xA82C, 0xA82C }, { 0xA8C4, 0xA8C5 }, { 0xA8E0, 0xA8F1 }, { 0xA8FF, 0xA8FF },
	{ 0xA926, 0xA92D }, { 0xA947, 0xA951 }, { 0xA980, 0xA982 }, { 0xA9B3, 0xA9BD },
	{ 0xA9E5, 0xA9E5 }, { 0xAA29, 0xAA36 }, { 0xAA43, 0xAA43 }, { 0xAA4C, 0xAA4C },
	{ 0xAA7C, 0xAA7C }, { 0xAAB0, 0xAAB0 }, { 0xAAB2, 0xAAB4 }, { 0xAAB7, 0xAAB8 },
	{ 0xAABE, 0xAABF }, { 0xAAC1, 0xAAC1 }, { 0xAAEC, 0xAAED }, { 0xAAF6, 0xAAF6 },
	{ 0xABE5, 0xABE8 }, { 0xABED, 0xABED }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
	{ 0xFE20, 0xFE2F },

	{ 0x101FD, 0x101FD }, { 0x102E0, 0x102E0 }, { 0x10376, 0x1037A },
	{ 0x10A01, 0x10A0F }, { 0x10A38, 0x10A3F }, { 0x10AE5, 0x10AE6 },
	{ 0x10D24, 0x10D27 }, { 0x10EAB, 0x10EAC }, { 0x10EFD, 0x10EFF },
	{ 0x10F46, 0x10F50 }, { 0x10F82, 0x10F85 }, { 0x11001, 0x11001 },
	{ 0x11038, 0x11046 }, { 0x11070, 0x11070 }, { 0x11073, 0x11074 },
	{ 0x1107F, 0x11081 }, { 0x110B3, 0x110BA }, { 0x110C2, 0x110C2 },
	{ 0x11100, 0x11102 }, { 0x11127, 0x11134 }, { 0x11173, 0x11173 },
	{ 0x11180, 0x11181 }, { 0x111B6, 0x111BE }, { 0x111C9, 0x111CC },
	{ 0x111CF, 0x111CF }, { 0x1122F, 0x11237 }, { 0x1123E, 0x1123E },
	{ 0x11241, 0x11241 }, { 0x112DF, 0x112EA }, { 0x11300, 0x11301 },
	{ 0x1133B, 0x1133C }, { 0x11340, 0x11340 }, { 0x11366, 0x11374 },
	{ 0x11438, 0x11446 }, { 0x1145E, 0x1145E }, { 0x114B3, 0x114C3 },
	{ 0x115B2, 0x115C0 }, { 0x115DC, 0x115DD }, { 0x11633, 0x11640 },
	{ 0x116AB, 0x116B7 }, { 0x1171D, 0x1172B }, { 0x1182F, 0x1183A },
	{ 0x1193B, 0x1193E }, { 0x11943, 0x11943 }, { 0x119D4, 0x119E0 },
	{ 0x11A01, 0x11A0A }, { 0x11A33, 0x11A38 }, { 0x11A3B, 0x11A3E },
	{ 0x11A47, 0x11A47 }, { 0x11A51, 0x11A5B }, { 0x11A8A, 0x11A99 },
	{ 0x11C30, 0x11C3F }, { 0x11C92, 0x11CB6 }, { 0x11D31, 0x11D45 },
	{ 0x11D47, 0x11D47 }, { 0x11D90, 0x11D97 }, { 0x11EF3, 0x11EF4 },
	{ 0x11F00, 0x11F01 }, { 0x11F36, 0x11F42 }, { 0x13440, 0x13440 },
	{ 0x13447, 0x13455 }, { 0x16AF0, 0x16AF4 }, { 0x16B30, 0x16B36 },
	{ 0x16F4F, 0x16F4F }, { 0x16F8F, 0x16F92 }, { 0x16FE4, 0x16FE4 },
	{ 0x1BC9D, 0x1BC9E }, { 0x1CF00, 0x1CF46 }, { 0x1D167, 0x1D169 },
	{ 0x1D17B, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
	{ 0x1D242, 0x1D244 }, { 0x1DA00, 0x1DA36 }, { 0x1DA3B, 0x1DA6C },
	{ 0x1DA75, 0x1DA75 }, { 0x1DA84, 0x1DA84 }, { 0x1DA9B, 0x1DAAF },
	{ 0x1E000, 0x1E02A }, { 0x1E08F, 0x1E08F }, { 0x1E130, 0x1E136 },
	{ 0x1E2AE, 0x1E2AE }, { 0x1E2EC, 0x1E2EF }, { 0x1E4EC, 0x1E4EF },
	{ 0x1E8D0, 0x1E8D6 }, { 0x1E944, 0x1E94A }
};

#define NUM_FULLWIDTH_RANGES  81
static const struct TweakCharPair  tw_table_fullwidths[NUM_FULLWIDTH_RANGES] =
{
	{ 0x1100, 0x115F }, { 0x231A, 0x231B }, { 0x2329, 0x232A }, { 0x23E9, 0x23EC },
	{ 0x23F0, 0x23F0 }, { 0x23F3, 0x23F3 }, { 0x25FD, 0x25FE }, { 0x2614, 0x2615 },
	{ 0x2648, 0x2653 }, { 0x267F, 0x267F }, { 0x2693, 0x2693 }, { 0x26A1, 0x26A1 },
	{ 0x26AA, 0x26AB }, { 0x26BD, 0x26BE }, { 0x26C4, 0x26C5 }, { 0x26CE, 0x26CE },
	{ 0x26D4, 0x26D4 }, { 0x26EA, 0x26EA }, { 0x26F2, 0x26F3 }, { 0x26F5, 0x26F5 },
	{ 0x26FA, 0x26FA }, { 0x26FD, 0x26FD }, { 0x2705, 0x2705 }, { 0x270A, 0x270B },
	{ 0x2728, 0x2728 }, { 0x274C, 0x274C }, { 0x274E, 0x274E }, { 0x2753, 0x2755 },
	{ 0x2757, 0x2757 }, { 0x2795, 0x2797 }, { 0x27B0, 0x27B0 }, { 0x27BF, 0x27BF },
	{ 0x2B1B, 0x2B1C }, { 0x2B50, 0x2B50 }, { 0x2B55, 0x2B55 }, { 0x2E80, 0x303E },
	{ 0x3041, 0x324F }, { 0x3250, 0x4DBF }, { 0x4E00, 0xA4C6 }, { 0xA960, 0xA97C },
	{ 0xAC00, 0xD7A3 }, { 0xF900, 0xFAFF }, { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE6B },
	{ 0xFF01, 0xFF60 }, { 0xFFE0, 0xFFE6 },

	{ 0x16FE0, 0x1B2FB }, { 0x1F004, 0x1F004 }, { 0x1F0CF, 0x1F0CF },
	{ 0x1F18E, 0x1F18E }, { 0x1F191, 0x1F19A }, { 0x1F200, 0x1F320 },
	{ 0x1F32D, 0x1F335 }, { 0x1F337, 0x1F37C }, { 0x1F37E, 0x1F393 },
	{ 0x1F3A0, 0x1F3CA }, { 0x1F3CF, 0x1F3D3 }, { 0x1F3E0, 0x1F3F0 },
	{ 0x1F3F4, 0x1F3F4 }, { 0x1F3F8, 0x1F43E }, { 0x1F440, 0x1F440 },
	{ 0x1F442, 0x1F4FC }, { 0x1F4FF, 0x1F53D }, { 0x1F54B, 0x1F54E },
	{ 0x1F550, 0x1F567 }, { 0x1F57A, 0x1F57A }, { 0x1F595, 0x1F596 },
	{ 0x1F5A4, 0x1F5A4 }, { 0x1F5FB, 0x1F64F }, { 0x1F680, 0x1F6C5 },
	{ 0x1F6CC, 0x1F6CC }, { 0x1F6D0, 0x1F6D2 }, { 0x1F6D5, 0x1F6DF },
	{ 0x1F6EB, 0x1F6EC }, { 0x1F6F4, 0x1F6FC }, { 0x1F7E0, 0x1F7F0 },
	{ 0x1F90C, 0x1F93A }, { 0x1F93C, 0x1F945 }, { 0x1F947, 0x1F9FF },
	{ 0x1FA70, 0x1FAF8 }, { 0x20000, 0x3FFFF }
};

//----------------------------------------------------------------------------

int tweak_utf8_encode_char (char * out, tw_char_t ch)
{
	// invalid?
	if (ch > 0x10FFFF)
		ch = 0xFFFD;

	if (ch > 0xFFFF)
	{
		// four bytes are required
		*out++ = ((ch >> 18) & 0x07) | 0xF0;
		*out++ = ((ch >> 12) & 0x3F) | 0x80;
		*out++ = ((ch >>  6) & 0x3F) | 0x80;
		*out   = ((ch      ) & 0x3F) | 0x80;

		return 4;
	}

	if (ch > 0x07FF)
	{
		// three bytes are required
		*out++ = ((ch >> 12) & 0x0F) | 0xE0;
		*out++ = ((ch >>  6) & 0x3F) | 0x80;
		*out   = ((ch      ) & 0x3F) | 0x80;

		return 3;
	}

	if (ch > 0x007F)
	{
		// two bytes are required
		*out++ = ((ch >>  6) & 0x1F) | 0xC0;
		*out   = ((ch      ) & 0x3F) | 0x80;

		return 2;
	}

	// just a single byte
	*out = ch;
	return 1;
}

int tweak_utf8_encode_str (char * out, int max_out, const tw_char_t * in)
{
	if (max_out <= 0)
		return -1;

	// ensure enough room for 4 bytes + a trailing NUL
	max_out -= 5;

	if (max_out <= 0)
	{
		out[0] = 0;
		return -1;
	}

	int len = 0;

	while (*in != 0)
	{
		if (len >= max_out)
		{
			*out = 0;
			return -1;
		}

		int got = tweak_utf8_encode_char (out, *in++);

		out += got;
		len += got;
	}

	*out = 0;
	return len;
}

int tweak_utf8_decode_char (tw_char_t * out, const char * s)
{
	tw_char_t ch = (unsigned char) *s++;

	if ((ch & 0xE0) == 0xC0)
	{
		// a two byte UTF-8 code
		if (s[0] == 0)
		{
			ch = 0xFFFD;
		}
		else
		{
			ch  = (ch & 0x1F) << 6;
			ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F);
		}

		*out = ch;
		return 2;
	}

	if ((ch & 0xF0) == 0xE0)
	{
		// a three byte UTF-8 code
		if (s[0] == 0 || s[1] == 0)
		{
			ch = 0xFFFD;
		}
		else
		{
			ch  = (ch & 0x0F) << 12;
			ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 6;
			ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F);
		}

		*out = ch;
		return 3;
	}

	if ((ch & 0xF8) == 0xF0)
	{
		// a four byte UTF-8 code
		if (s[0] == 0 || s[1] == 0 || s[2] == 0)
		{
			ch = 0xFFFD;
		}
		else
		{
			ch  = (ch & 0x07) << 18;
			ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 12;
			ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F) << 6;
			ch |= (tw_char_t) ((unsigned char) s[2] & 0x3F);
		}

		*out = ch;
		return 4;
	}

	if (ch >= 0x80)
	{
		// invalid UTF-8
		ch = 0xFFFD;
	}

	*out = ch;
	return 1;
}

int tweak_utf8_decode_str (tw_char_t * out, int max_out, const char * s)
{
	if (max_out <= 0)
		return -1;

	// save room for the trailing NUL
	max_out--;

	int count = 0;

	while (*s != 0)
	{
		if (max_out <= 0)
		{
			*out = 0;
			return -1;
		}

		tw_char_t ch = (unsigned char) *s++;

		if ((ch & 0xE0) == 0xC0)
		{
			// a two byte UTF-8 code
			if (s[0] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x1F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F);

				s++;
			}
		}
		else if ((ch & 0xF0) == 0xE0)
		{
			// a three byte UTF-8 code
			if (s[0] == 0 || s[1] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x0F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F);

				s += 2;
			}
		}
		else if ((ch & 0xF8) == 0xF0)
		{
			// a four byte UTF-8 code
			if (s[0] == 0 || s[1] == 0 || s[2] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x07) << 18;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[2] & 0x3F);

				s += 3;
			}
		}
		else if (ch >= 0x80)
		{
			// invalid UTF-8
			ch = 0xFFFD;
		}

		*out++ = ch;

		count   += 1;
		max_out -= 1;
	}

	*out = 0;
	return count;
}

int tweak_utf8_to_utf16 (unsigned short * out, int max_out, const char * s)
{
	if (max_out <= 0)
		return -1;

	// save room for the trailing NUL
	max_out--;

	int count = 0;

	while (*s != 0)
	{
		// need room for a surrogate pair
		if (max_out < 2)
		{
			*out = 0;
			return -1;
		}

		tw_char_t ch = (unsigned char) *s++;

		if ((ch & 0xE0) == 0xC0)
		{
			// a two byte UTF-8 code
			if (s[0] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x1F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F);

				s++;
			}
		}
		else if ((ch & 0xF0) == 0xE0)
		{
			// a three byte UTF-8 code
			if (s[0] == 0 || s[1] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x0F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F);

				s += 2;
			}
		}
		else if ((ch & 0xF8) == 0xF0)
		{
			// a four byte UTF-8 code
			if (s[0] == 0 || s[1] == 0 || s[2] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x07) << 18;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[2] & 0x3F);

				s += 3;
			}
		}
		else if (ch >= 0x80)
		{
			// invalid UTF-8
			ch = 0xFFFD;
		}

		if (0xD800 <= ch && ch <= 0xDFFF)
			ch = 0xFFFD;

		if (ch < 0xFFFF)
		{
			*out++ = ch;

			count   += 1;
			max_out -= 1;
		}
		else  // convert to a surrogate pair
		{
			ch -= 0x10000;

			*out++ = 0xD800 | ((ch >> 10) & 0x3FF);
			*out++ = 0xDC00 | ((ch)       & 0x3FF);

			count   += 2;
			max_out -= 2;
		}
	}

	*out = 0;
	return count;
}

int tweak_utf8_from_utf16 (char * out, int max_out, const unsigned short * in)
{
	if (max_out <= 0)
		return -1;

	// ensure enough room for 4 bytes + a trailing NUL
	max_out -= 5;

	if (max_out <= 0)
	{
		out[0] = 0;
		return -1;
	}

	int len = 0;

	while (*in != 0)
	{
		if (len >= max_out)
		{
			*out = 0;
			return -1;
		}

		tw_char_t ch = *in++;

		// handle surrogate pairs, including invalid ones
		if (0xD800 <= ch && ch <= 0xDBFF)
		{
			tw_char_t high = ch;
			tw_char_t low  = *in;

			if (0xDC00 <= low && low <= 0xDFFF)
			{
				ch = 0x10000 + ((high & 0x3FF) << 10) + (low & 0x3FF);
				in++;
			}
			else
			{
				ch = 0xFFFD;
			}
		}
		else if (0xDC00 <= ch && ch <= 0xDFFF)
		{
			ch = 0xFFFD;
		}

		int got = tweak_utf8_encode_char (out, ch);

		out += got;
		len += got;
	}

	*out = 0;
	return len;
}

int tweak_utf8_to_utf32 (tw_char_t * out, int max_out, const char * s)
{
	if (max_out <= 0)
		return -1;

	// save room for the trailing NUL
	max_out--;

	int count = 0;

	while (*s != 0)
	{
		if (max_out < 1)
		{
			*out = 0;
			return -1;
		}

		tw_char_t ch = (unsigned char) *s++;

		if ((ch & 0xE0) == 0xC0)
		{
			// a two byte UTF-8 code
			if (s[0] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x1F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F);

				s++;
			}
		}
		else if ((ch & 0xF0) == 0xE0)
		{
			// a three byte UTF-8 code
			if (s[0] == 0 || s[1] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x0F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F);

				s += 2;
			}
		}
		else if ((ch & 0xF8) == 0xF0)
		{
			// a four byte UTF-8 code
			if (s[0] == 0 || s[1] == 0 || s[2] == 0)
			{
				ch = 0xFFFD;
			}
			else
			{
				ch  = (ch & 0x07) << 18;
				ch |= (tw_char_t) ((unsigned char) s[0] & 0x3F) << 12;
				ch |= (tw_char_t) ((unsigned char) s[1] & 0x3F) << 6;
				ch |= (tw_char_t) ((unsigned char) s[2] & 0x3F);

				s += 3;
			}
		}
		else if (ch >= 0x80)
		{
			// invalid UTF-8
			ch = 0xFFFD;
		}

		// surrogate pairs are invalid in UTF-32
		if (0xD800 <= ch && ch <= 0xDFFF)
			ch = 0xFFFD;

		*out++ = ch;

		count   += 1;
		max_out -= 1;
	}

	*out = 0;
	return count;
}

int tweak_utf8_from_utf32 (char * out, int max_out, const tw_char_t * in)
{
	if (max_out <= 0)
		return -1;

	// ensure enough room for 4 bytes + a trailing NUL
	max_out -= 5;

	if (max_out <= 0)
	{
		out[0] = 0;
		return -1;
	}

	int len = 0;

	while (*in != 0)
	{
		if (len >= max_out)
		{
			*out = 0;
			return -1;
		}

		tw_char_t ch = *in++;

		// surrogate pairs are invalid in UTF-32
		if (0xD800 <= ch && ch <= 0xDFFF)
			ch = 0xFFFD;

		int got = tweak_utf8_encode_char (out, ch);

		out += got;
		len += got;
	}

	*out = 0;
	return len;
}

int tweak_utf8_fputc (tw_char_t ch, FILE * fp)
{
	char out_buf[16];
	int  len = tweak_utf8_encode_char (out_buf, ch);

	int  i;
	for (i = 0 ; i < len ; i++)
	{
		fputc (out_buf[i], fp);
	}

	return len;
}

tw_char_t tweak_utf8_fgetc (FILE * fp)
{
	if (ferror (fp) || feof (fp))
		return TW_EOF;

	int ch = fgetc (fp);
	if (ch == EOF)
		return TW_EOF;

	// check for UTF-8...
	int want_len = tweak_utf8_char_length ((unsigned char)ch);
	if (want_len < 2)
		return (tw_char_t) ch;

	// we need more bytes for the UTF-8 encoded character
	char in_buf[16];

	in_buf[0] = ch;

	int len;
	for (len = 1 ; len < want_len ; len++)
	{
		ch = fgetc (fp);
		if (ch == EOF)
			return 0xFFFD;

		in_buf[len] = ch;
	}

	// decode the UTF-8 character
	tw_char_t out_buf[4];
	tweak_utf8_decode_char (out_buf, in_buf);

	return out_buf[0];
}

int tweak_utf8_char_length (char first)
{
	if (((unsigned char)first & 0xE0) == 0xC0)
		return 2;

	if (((unsigned char)first & 0xF0) == 0xE0)
		return 3;

	if (((unsigned char)first & 0xF8) == 0xF0)
		return 4;

	return 1;
}

int tweak_detect_bom (unsigned char * data, int length)
{
	// if everybody's got a BOM, we could all die any day...

	if (length >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
		return TW_BOM_UTF_8;

	if (length >= 2 && data[0] == 0xFF && data[1] == 0xFE)
		return TW_BOM_UTF_16;

	if (length >= 2 && data[0] == 0xFE && data[1] == 0xFF)
		return TW_BOM_UTF_16_BE;

	return TW_BOM_NONE;
}

int tweak_wide_strlen (const tw_char_t * s)
{
	const tw_char_t * begin = s;

	while (*s != 0)
		s++;

	return (int)(s - begin);
}

//----------------------------------------------------------------------------

static int tweak_is_in_table (tw_char_t ch, const tw_char_t * tab, int num)
{
	while (num > 2)
	{
		int mid = num / 2;

		if (ch < tab[mid])
			num = num / 2;
		else
			tab += mid, num = (num + 1) / 2;
	}

	return (ch == tab[0]) || (num == 2 && ch == tab[1]);
}

static int tweak_is_in_pair_table (tw_char_t ch, const struct TweakCharPair * tab, int num)
{
	while (num > 2)
	{
		int mid = num / 2;

		if (ch < tab[mid].low)
			num = num / 2;
		else if (ch > tab[mid].high)
			tab += (mid + 1), num = (num - 1) / 2;
		else
			return 1;
	}

	if (ch <  tab[0].low)  return 0;
	if (ch <= tab[0].high) return 1;

	if (num == 2)
	{
		if (ch <  tab[1].low)  return 0;
		if (ch <= tab[1].high) return 1;
	}

	return 0;
}

int tweak_char_width (tw_char_t ch)
{
	// the NUL character has zero width
	if (ch == 0)
		return 0;

	// C0 and C1 control codes are special, produce a zero result
	if (ch < 32 || (127 <= ch && ch < 160))
		return 0;

	// the soft hyphen is either drawn or hidden depending on whether
	// the word containing it is split at the end of a line.
	// so the choice of width is a bit arbitrary...
	if (ch == 0xAD)
		return 1;

	// NOTE: the following order of tests is important, because the above
	//       tables were reduced in size by assuming this particular order
	//       (and being allowed to cover non-assigned unicode code-points).

	if (tweak_is_in_table (ch, tw_table_zero_width, NUM_ZERO_WIDTHS))
		return 0;

	// some combining characters (in `Mc` category) make the base
	// character significantly wider.
	if (tweak_is_in_table (ch, tw_table_compose_spacing, NUM_COMPOSE_SPACINGS))
		return 1;

	// other combining characters (in `Mn` category) just make the base
	// character taller.  the `Me` category is for enclosing forms, which
	// (in the context of terminals) don't significantly affect the width.
	if (tweak_is_in_pair_table (ch, tw_table_composing, NUM_COMPOSING_RANGES))
		return 0;

	// is it a fullwidth (CJK) character?
	if (tweak_is_in_pair_table (ch, tw_table_fullwidths, NUM_FULLWIDTH_RANGES))
		return 2;

	// otherwise it is narrow
	return 1;
}

int tweak_char_is_compose (tw_char_t ch)
{
	if (tweak_is_in_table (ch, tw_table_compose_spacing, NUM_COMPOSE_SPACINGS))
		return 2;

	if (tweak_is_in_pair_table (ch, tw_table_composing, NUM_COMPOSING_RANGES))
		return 1;

	return 0;
}

#endif /* TWEAK_IMPL */
