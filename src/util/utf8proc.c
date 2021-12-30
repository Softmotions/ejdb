/* -*- mode: c; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*- */
/*
 *  Copyright (c) 2015 Steven G. Johnson, Jiahao Chen, Peter Colberg, Tony Kelman, Scott P. Jones, and other
 * contributors.
 *  Copyright (c) 2009 Public Software Group e. V., Berlin, Germany
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

/*
 *  This library contains derived data from a modified version of the
 *  Unicode data files.
 *
 *  The original data files are available at
 *  http://www.unicode.org/Public/UNIDATA/
 *
 *  Please notice the copyright statement in the file "utf8proc_data.c".
 */


/*
 *  File name:    utf8proc.c
 *
 *  Description:
 *  Implementation of libutf8proc.
 */


#include "utf8proc.h"

UTF8PROC_DLLEXPORT const utf8proc_int8_t utf8proc_utf8class[256] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0
};

#define UTF8PROC_HANGUL_SBASE  0xAC00
#define UTF8PROC_HANGUL_LBASE  0x1100
#define UTF8PROC_HANGUL_VBASE  0x1161
#define UTF8PROC_HANGUL_TBASE  0x11A7
#define UTF8PROC_HANGUL_LCOUNT 19
#define UTF8PROC_HANGUL_VCOUNT 21
#define UTF8PROC_HANGUL_TCOUNT 28
#define UTF8PROC_HANGUL_NCOUNT 588
#define UTF8PROC_HANGUL_SCOUNT 11172
/* END is exclusive */
#define UTF8PROC_HANGUL_L_START  0x1100
#define UTF8PROC_HANGUL_L_END    0x115A
#define UTF8PROC_HANGUL_L_FILLER 0x115F
#define UTF8PROC_HANGUL_V_START  0x1160
#define UTF8PROC_HANGUL_V_END    0x11A3
#define UTF8PROC_HANGUL_T_START  0x11A8
#define UTF8PROC_HANGUL_T_END    0x11FA
#define UTF8PROC_HANGUL_S_START  0xAC00
#define UTF8PROC_HANGUL_S_END    0xD7A4

/* Should follow semantic-versioning rules (semver.org) based on API
   compatibility.  (Note that the shared-library version number will
   be different, being based on ABI compatibility.): */
#define STRINGIZEx(x) #x
#define STRINGIZE(x)  STRINGIZEx(x)

UTF8PROC_DLLEXPORT const char* utf8proc_version(void) {
  return STRINGIZE(UTF8PROC_VERSION_MAJOR) "." STRINGIZE(UTF8PROC_VERSION_MINOR) "." STRINGIZE(UTF8PROC_VERSION_PATCH)
         "";
}

UTF8PROC_DLLEXPORT const char* utf8proc_errmsg(utf8proc_ssize_t errcode) {
  switch (errcode) {
    case UTF8PROC_ERROR_NOMEM:
      return "Memory for processing UTF-8 data could not be allocated.";
    case UTF8PROC_ERROR_OVERFLOW:
      return "UTF-8 string is too long to be processed.";
    case UTF8PROC_ERROR_INVALIDUTF8:
      return "Invalid UTF-8 string";
    case UTF8PROC_ERROR_NOTASSIGNED:
      return "Unassigned Unicode code point found in UTF-8 string.";
    case UTF8PROC_ERROR_INVALIDOPTS:
      return "Invalid options for UTF-8 processing chosen.";
    default:
      return "An unknown error occurred while processing UTF-8 data.";
  }
}

#define utf_cont(ch) (((ch) & 0xc0) == 0x80)

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_iterate(
  const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_int32_t *dst
  ) {
  utf8proc_uint32_t uc;
  const utf8proc_uint8_t *end;

  *dst = -1;
  if (!strlen) {
    return 0;
  }
  end = str + ((strlen < 0) ? 4 : strlen);
  uc = *str++;
  if (uc < 0x80) {
    *dst = uc;
    return 1;
  }
  // Must be between 0xc2 and 0xf4 inclusive to be valid
  if ((uc - 0xc2) > (0xf4 - 0xc2)) {
    return UTF8PROC_ERROR_INVALIDUTF8;
  }
  if (uc < 0xe0) {         // 2-byte sequence
    // Must have valid continuation character
    if ((str >= end) || !utf_cont(*str)) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
    *dst = ((uc & 0x1f) << 6) | (*str & 0x3f);
    return 2;
  }
  if (uc < 0xf0) {        // 3-byte sequence
    if ((str + 1 >= end) || !utf_cont(*str) || !utf_cont(str[1])) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
    // Check for surrogate chars
    if ((uc == 0xed) && (*str > 0x9f)) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
    uc = ((uc & 0xf) << 12) | ((*str & 0x3f) << 6) | (str[1] & 0x3f);
    if (uc < 0x800) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
    *dst = uc;
    return 3;
  }
  // 4-byte sequence
  // Must have 3 valid continuation characters
  if ((str + 2 >= end) || !utf_cont(*str) || !utf_cont(str[1]) || !utf_cont(str[2])) {
    return UTF8PROC_ERROR_INVALIDUTF8;
  }
  // Make sure in correct range (0x10000 - 0x10ffff)
  if (uc == 0xf0) {
    if (*str < 0x90) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
  } else if (uc == 0xf4) {
    if (*str > 0x8f) {
      return UTF8PROC_ERROR_INVALIDUTF8;
    }
  }
  *dst = ((uc & 7) << 18) | ((*str & 0x3f) << 12) | ((str[1] & 0x3f) << 6) | (str[2] & 0x3f);
  return 4;
}

UTF8PROC_DLLEXPORT utf8proc_bool utf8proc_codepoint_valid(utf8proc_int32_t uc) {
  return (((utf8proc_uint32_t) uc) - 0xd800 > 0x07ff) && ((utf8proc_uint32_t) uc < 0x110000);
}

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_encode_char(utf8proc_int32_t uc, utf8proc_uint8_t *dst) {
  if (uc < 0x00) {
    return 0;
  } else if (uc < 0x80) {
    dst[0] = (utf8proc_uint8_t) uc;
    return 1;
  } else if (uc < 0x800) {
    dst[0] = (utf8proc_uint8_t) (0xC0 + (uc >> 6));
    dst[1] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 2;
    // Note: we allow encoding 0xd800-0xdfff here, so as not to change
    // the API, however, these are actually invalid in UTF-8
  } else if (uc < 0x10000) {
    dst[0] = (utf8proc_uint8_t) (0xE0 + (uc >> 12));
    dst[1] = (utf8proc_uint8_t) (0x80 + ((uc >> 6) & 0x3F));
    dst[2] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 3;
  } else if (uc < 0x110000) {
    dst[0] = (utf8proc_uint8_t) (0xF0 + (uc >> 18));
    dst[1] = (utf8proc_uint8_t) (0x80 + ((uc >> 12) & 0x3F));
    dst[2] = (utf8proc_uint8_t) (0x80 + ((uc >> 6) & 0x3F));
    dst[3] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 4;
  } else {
    return 0;
  }
}

/* internal "unsafe" version that does not check whether uc is in range */
static utf8proc_ssize_t unsafe_encode_char(utf8proc_int32_t uc, utf8proc_uint8_t *dst) {
  if (uc < 0x00) {
    return 0;
  } else if (uc < 0x80) {
    dst[0] = (utf8proc_uint8_t) uc;
    return 1;
  } else if (uc < 0x800) {
    dst[0] = (utf8proc_uint8_t) (0xC0 + (uc >> 6));
    dst[1] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 2;
  } else if (uc == 0xFFFF) {
    dst[0] = (utf8proc_uint8_t) 0xFF;
    return 1;
  } else if (uc == 0xFFFE) {
    dst[0] = (utf8proc_uint8_t) 0xFE;
    return 1;
  } else if (uc < 0x10000) {
    dst[0] = (utf8proc_uint8_t) (0xE0 + (uc >> 12));
    dst[1] = (utf8proc_uint8_t) (0x80 + ((uc >> 6) & 0x3F));
    dst[2] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 3;
  } else if (uc < 0x110000) {
    dst[0] = (utf8proc_uint8_t) (0xF0 + (uc >> 18));
    dst[1] = (utf8proc_uint8_t) (0x80 + ((uc >> 12) & 0x3F));
    dst[2] = (utf8proc_uint8_t) (0x80 + ((uc >> 6) & 0x3F));
    dst[3] = (utf8proc_uint8_t) (0x80 + (uc & 0x3F));
    return 4;
  } else {
    return 0;
  }
}

/* return whether there is a grapheme break between boundclasses lbc and tbc
   (according to the definition of extended grapheme clusters)

   Rule numbering refers to TR29 Version 29 (Unicode 9.0.0):
   http://www.unicode.org/reports/tr29/tr29-29.html

   CAVEATS:
   Please note that evaluation of GB10 (grapheme breaks between emoji zwj sequences)
   and GB 12/13 (regional indicator code points) require knowledge of previous characters
   and are thus not handled by this function. This may result in an incorrect break before
   an E_Modifier class codepoint and an incorrectly missing break between two
   REGIONAL_INDICATOR class code points if such support does not exist in the caller.

   See the special support in grapheme_break_extended, for required bookkeeping by the caller.
 */
static utf8proc_bool grapheme_break_simple(int lbc, int tbc) {
  return (lbc == UTF8PROC_BOUNDCLASS_START) ? true                                      // GB1
         : (  lbc == UTF8PROC_BOUNDCLASS_CR                                             // GB3
           && tbc == UTF8PROC_BOUNDCLASS_LF) ? false                                    // ---
         : (lbc >= UTF8PROC_BOUNDCLASS_CR && lbc <= UTF8PROC_BOUNDCLASS_CONTROL) ? true // GB4
         : (tbc >= UTF8PROC_BOUNDCLASS_CR && tbc <= UTF8PROC_BOUNDCLASS_CONTROL) ? true // GB5
         : (  lbc == UTF8PROC_BOUNDCLASS_L                                              // GB6
           && (  tbc == UTF8PROC_BOUNDCLASS_L                                           // ---
              || tbc == UTF8PROC_BOUNDCLASS_V                                           // ---
              || tbc == UTF8PROC_BOUNDCLASS_LV                                          // ---
              || tbc == UTF8PROC_BOUNDCLASS_LVT)) ? false                               // ---
         : (  (  lbc == UTF8PROC_BOUNDCLASS_LV                                          // GB7
              || lbc == UTF8PROC_BOUNDCLASS_V)                                          // ---
           && (  tbc == UTF8PROC_BOUNDCLASS_V                                           // ---
              || tbc == UTF8PROC_BOUNDCLASS_T)) ? false                                 // ---
         : (  (  lbc == UTF8PROC_BOUNDCLASS_LVT                                         // GB8
              || lbc == UTF8PROC_BOUNDCLASS_T)                                          // ---
           && tbc == UTF8PROC_BOUNDCLASS_T) ? false                                     // ---
         : (  tbc == UTF8PROC_BOUNDCLASS_EXTEND                                         // GB9
           || tbc == UTF8PROC_BOUNDCLASS_ZWJ                                            // ---
           || tbc == UTF8PROC_BOUNDCLASS_SPACINGMARK                                    // GB9a
           || lbc == UTF8PROC_BOUNDCLASS_PREPEND) ? false                               // GB9b
         : (  (  lbc == UTF8PROC_BOUNDCLASS_E_BASE                                      // GB10 (requires additional
                                                                                        // handling below)
              || lbc == UTF8PROC_BOUNDCLASS_E_BASE_GAZ)                                 // ----
           && tbc == UTF8PROC_BOUNDCLASS_E_MODIFIER) ? false                            // ----
         : (  lbc == UTF8PROC_BOUNDCLASS_ZWJ                                            // GB11
           && (  tbc == UTF8PROC_BOUNDCLASS_GLUE_AFTER_ZWJ                              // ----
              || tbc == UTF8PROC_BOUNDCLASS_E_BASE_GAZ)) ? false                        // ----
         : (  lbc == UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR                             // GB12/13 (requires additional
                                                                                        // handling below)
           && tbc == UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR) ? false                    // ----
         : true;                                                                        // GB999
}

static utf8proc_bool grapheme_break_extended(int lbc, int tbc, utf8proc_int32_t *state) {
  int lbc_override = ((state && *state != UTF8PROC_BOUNDCLASS_START)
                      ? *state : lbc);
  utf8proc_bool break_permitted = grapheme_break_simple(lbc_override, tbc);
  if (state) {
    // Special support for GB 12/13 made possible by GB999. After two RI
    // class codepoints we want to force a break. Do this by resetting the
    // second RI's bound class to UTF8PROC_BOUNDCLASS_OTHER, to force a break
    // after that character according to GB999 (unless of course such a break is
    // forbidden by a different rule such as GB9).
    if ((*state == tbc) && (tbc == UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR)) {
      *state = UTF8PROC_BOUNDCLASS_OTHER;
    }
    // Special support for GB10. Fold any EXTEND codepoints into the previous
    // boundclass if we're dealing with an emoji base boundclass.
    else if (  (  (*state == UTF8PROC_BOUNDCLASS_E_BASE)
               || (*state == UTF8PROC_BOUNDCLASS_E_BASE_GAZ))
            && (tbc == UTF8PROC_BOUNDCLASS_EXTEND)) {
      *state = UTF8PROC_BOUNDCLASS_E_BASE;
    } else {
      *state = tbc;
    }
  }
  return break_permitted;
}

static utf8proc_int32_t seqindex_decode_entry(const utf8proc_uint16_t **entry) {
  utf8proc_int32_t entry_cp = **entry;
  if ((entry_cp & 0xF800) == 0xD800) {
    *entry = *entry + 1;
    entry_cp = ((entry_cp & 0x03FF) << 10) | (**entry & 0x03FF);
    entry_cp += 0x10000;
  }
  return entry_cp;
}
