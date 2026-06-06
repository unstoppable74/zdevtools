// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

bool char_is_letter (uchar_t ch)
{
	// WISH: full unicode support

	// the classification here is "greedy", while anything which is
	// usually considered a letter is treated as one, many characters
	// which are *not* considered a letter are also treated as one.

	if ('A' <= ch && ch <= 'Z') return true;
	if ('a' <= ch && ch <= 'z') return true;

	if (ch <  0x00C0) return false;
	if (ch == 0x00D7) return false;  // multiply sign
	if (ch == 0x00F7) return false;  // divide sign

	if (0x00C0 <= ch && ch <= 0x00FF) return true;   // Latin 1
	if (0x0100 <= ch && ch <= 0x017F) return true;   // Latin Ext A
	if (0x0180 <= ch && ch <= 0x023F) return true;   // Latin Ext B

	if (0x2000 <= ch && ch <= 0x2BFF) return false;
	if (0xD800 <= ch)                 return false;

	return ! char_is_space (ch);
}

bool char_is_digit (uchar_t ch)
{
	// this deliberately only covers the ASCII digits

	if ('0' <= ch && ch <= '9')
		return true;

	return false;
}

bool char_is_hex_digit (uchar_t ch)
{
	if ('0' <= ch && ch <= '9') return true;
	if ('a' <= ch && ch <= 'f') return true;
	if ('A' <= ch && ch <= 'F') return true;

	return false;
}

bool char_is_symbol (uchar_t ch)
{
	if (char_is_letter (ch)) return false;
	if (char_is_digit  (ch)) return false;
	if (char_is_space  (ch)) return false;

	return true;
}

bool char_is_space (uchar_t ch)
{
	// note that `NUL` is a control char, hence it is considered
	// whitespace here.

	if (ch == ' ')  return true;
	if (ch == 0x7F) return true;  // DEL
	if (ch == 0xA0) return true;  // non-break space

	if (ch < 0x0020)                  return true;  // C0 control chars
	if (0x0080 <= ch && ch <= 0x009F) return true;  // C1 control chars
	if (0x2000 <= ch && ch <= 0x200B) return true;  // various

	if (ch == 0x2028) return true;  // line separator
	if (ch == 0x2029) return true;  // paragraph separator
	if (ch == 0x205F) return true;  // mathematical space
	if (ch == 0x3000) return true;  // ideographic space
	if (ch == 0xFEFF) return true;  // zero-width space

	return false;
}
