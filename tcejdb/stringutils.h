/*-
 * Copyright (c) 2008 - 2010 CAS Dev Team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the CAS Dev. Team nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      stringutils.h
 *
 * $CAS$
 */
#ifndef _STRINGUTILS_H__
#define _STRINGUTILS_H__ 1

/*
  @file stringutils.h
  @brief UTF8 strings functions
*/

/**
  @fn int utf8strlen(const char * szSrc);
  @brief Find length of NULL-terminated UTF8 string
  @param szSrc - Source UTF8 string
  @param iSrcLen - Source string length
  @return >0 (number of utf8 characters) if success, -1 - if incorrect input character found
*/
int utf8strlen(const char * szSrc);

/**
  @fn int utf8strlen(const char * szSrc, int iSrcLen);
  @brief Find length of UTF8 string, check not more than iSrcLen bytes.
  @param szSrc - Source UTF8 string
  @param iSrcLen - Source string length
  @return >0 (number of utf8 characters) if success, -1 - if incorrect input character found
*/
int utf8strnlen(const char * szSrc, int iSrcLen);

/**
  @fn int utf8strncmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len);
  @brief Compare two utf-8 strings
  @param szSrc1 - First UTF8 string
  @param iSrc1Len - First string length
  @param szSrc2 - Second UTF8 string
  @param iSrc2Len - Second string length
  @return +1, 0, -1 according as szSrc1 is lexicographically greater than, equal to, or less than szSrc2. If function return +2 or -2 it means that first or second string contains incorrct UTF8 characters
*/
int utf8strncmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len);

/**
  @fn int utf8strncasecmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len);
  @brief Compare two utf-8 strings, ignoring case
  @param szSrc1 - First UTF8 string
  @param iSrc1Len - First string length
  @param szSrc2 - Second UTF8 string
  @param iSrc2Len - Second string length
   @return +1, 0, -1 according as szSrc1 is lexicographically greater than, equal to, or less than szSrc2 after translation of each corresponding character to lower-case. If function return -2 or +2 it means that first or second string contains incorrect UTF8 characters
*/
int utf8strncasecmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len);

#endif /* _STRINGUTILS_H__ */
/* End. */
