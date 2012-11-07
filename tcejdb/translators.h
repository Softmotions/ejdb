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
 *      translators.h
 *
 * $CAS$
 */
#ifndef _TRANSLATORS_H__
#define _TRANSLATORS_H__ 1

/*
  @file translatros.h
  @brief Uppercase/lowercase manipulation functions
*/

/**
  @fn int wtoutf8(unsigned int iUCS, char * sUTF8);
  @brief convert UTF8 character to wide
  @param iUCS - UCS character
  @param sUTF8 - UTF8 string, at least 6 bytes length
  @return size of UTF8 character if success or -1 otherwise
*/
int wtoutf8(unsigned int iUCS, char * sUTF8);

/**
  @fn int utf8tow(const char * sUTF8, int iUTF8Length, unsigned int * iUCSResult);
  @brief convert character to UTF8
  @param sUTF8 - String with UTF8 characters
  @param iUTF8Length - max. string length
  @param iUCSResult - UCS character
  @return size of UTF8 character if success or -1 otherwise
*/
int utf8tow(const char * sUTF8, int iUTF8Length, unsigned int * iUCSResult);

/**
  @fn int utf8lcstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen);
  @brief Make a UTF8 string lowercase
  @param szSrc - Source UTF8 string
  @param iSrcLen - Source string length
  @param szDst - Destination UTF8 string
  @param iDstLen - Destination string length
  @return 0 - if success, -1 - if incorrect input character found,  -2 - if incorrect lowercase character found in translation table ; -3 - if can't reallocate memory.
*/
int utf8lcstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen);

/**
  @fn int utf8ucstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen);
  @brief Make a UTF8 string uppercase
  @param szSrc - Source UTF8 string
  @param iSrcLen - Source string length
  @param szDst - Destination UTF8 string
  @param iDstLen - Destination string length
  @return 0 - if success, -1 - if incorrect input character found,  -2 - if incorrect lowercase character found in translation table ; -3 - if can't reallocate memory.
*/
int utf8ucstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen);

#endif /* _TRANSLATORS_H__ */
/* End. */
