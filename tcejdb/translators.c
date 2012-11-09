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
 *      translators.c
 *
 * $CAS$
 */

#include "translators.h"
#include "chartables.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>

/**
  @var typedef unsigned int ((*CaseFn)(unsigned int));
 */
typedef unsigned int ((*CaseFn)(unsigned int));

/*
 * Convert UTF8 character to wide
 */
int wtoutf8(unsigned int iUCS, char * sUTF8) {
    int iPos = 0;
    int iCharLength = 0;
    unsigned char sUTF8Prefix = 0;

    /* ASCII characters. */
    if ((iUCS & ~0x0000007F) == 0) {
        /* Modified UTF-8, special case */
        if (iUCS == 0) {
            sUTF8[0] = (char) 0xC0;
            sUTF8[1] = (char) 0x80;
            return 2;
        }

        sUTF8[0] = (char) iUCS;
        return 1;
    }
    /*
     * Does not need here, but let it be
        if      ((iUCS & ~0x0000007F) == 0)
        {
            sUTF8Prefix = 0x00; // 11000000b
            iCharLength = 1;
        } else
     */

    if ((iUCS & ~0x000007FF) == 0) {
        sUTF8Prefix = 0xC0; // 11000000b
        iCharLength = 2;
    } else if ((iUCS & ~0x0000FFFF) == 0) {
        sUTF8Prefix = 0xE0; // 11100000b
        iCharLength = 3;
    } else if ((iUCS & ~0x001FFFFF) == 0) {
        sUTF8Prefix = 0xF0; // 11110000b
        iCharLength = 4;
    } else if ((iUCS & ~0x03FFFFFF) == 0) {
        sUTF8Prefix = 0xF8; // 11111000b
        iCharLength = 5;
    } else if ((iUCS & ~0x7FFFFFFF) == 0) {
        sUTF8Prefix = 0xFC; // 11111100b
        iCharLength = 6;
    }/* Incorrect multibyte character */
    else {
        return -1;
    }

    /*
     * Convert UCS character to UTF8. Split value in 6-bit chunks and
     * move to UTF8 string
     */
    for (iPos = iCharLength - 1; iPos > 0; --iPos) {
        sUTF8[iPos] = (iUCS & 0x0000003F) | 0x80;
        iUCS >>= 6;
    }

    /* UTF8 prefix, special case */
    sUTF8[0] = (iUCS & 0x000000FF) | sUTF8Prefix;

    /* Return size of UTF8 character */
    return iCharLength;
}

/*
 * Convert character to UTF8
 */
int utf8tow(const char * sUTF8, int iUTF8Length, unsigned int * iUCSResult) {
    unsigned int iUCS = 0;
    int iPos = 0;
    int iCharLength = 0;
    unsigned char ucPrefix = 0;
    unsigned int uBoundary = 0;

    /* Incorrect multibyte sequence */
    if (iUTF8Length == 0 || sUTF8 == NULL) {
        return -1;
    }

    /*
      Determine size of UTF8 character & check "shortest form" of char
     */
    ucPrefix = (unsigned char) (*sUTF8);

    /* 10000000b  & 00000000b */
    if ((ucPrefix & 0x80) == 0) {
        ucPrefix = 0x7F;
        iCharLength = 1;
        uBoundary = 0;
    }/* 11100000b & 11000000b */
    else if ((ucPrefix & 0xE0) == 0xC0) {
        ucPrefix = 0x1F;
        iCharLength = 2;
        uBoundary = 0x80;
    }/* 11110000b & 11100000b */
    else if ((ucPrefix & 0xF0) == 0xE0) {
        ucPrefix = 0x0F;
        iCharLength = 3;
        uBoundary = 0x00000800;
    }/* 11111000b & 11110000b */
    else if ((ucPrefix & 0xF8) == 0xF0) {
        ucPrefix = 0x07;
        iCharLength = 4;
        uBoundary = 0x00010000;
    }/* 11111100b & 11111000b */
    else if ((ucPrefix & 0xFC) == 0xF8) {
        ucPrefix = 0x03;
        iCharLength = 5;
        uBoundary = 0x00200000;
    }/* 11111110b & 11111100b */
    else if ((ucPrefix & 0xFE) == 0xFC) {
        ucPrefix = 0x01;
        iCharLength = 6;
        uBoundary = 0x04000000;
    }/* Invalid UTF8 sequence */
    else {
        return -1;
    }

    /* Invalid UTF8 sequence */
    if (iUTF8Length < iCharLength) {
        return -1;
    }

    /* Special case for first character */
    iUCS = (unsigned char) (*sUTF8) & ucPrefix;
    ++sUTF8;
    for (iPos = 1; iPos < iCharLength; ++iPos) {
        /* Incorect characters in middle of string */
        if (((*sUTF8) & 0xC0) != 0x80) {
            return -1;
        }

        /* Join value from 6-bit chunks */
        iUCS <<= 6;
        iUCS |= (*sUTF8) & 0x3F;
        ++sUTF8;
    }

    /* Check boundary */
    if (iUCS < uBoundary) {
        /* Modified UTF-8, special case */
        if (!(iUCS == 0 && uBoundary == 0x80)) {
            /* Not a "shortest form" of char */
            return -1;
        }
    }

    *iUCSResult = iUCS;

    return iCharLength;
}

/*
 * Make a UTF8 string upper or lowercase
 */
int changeutf8case(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen, CaseFn pCaseFn) {
    int iAllocated = iSrcLen;
    char * szTMPDst = (char*) malloc(sizeof (char) * iAllocated);
    int iRealDstLen = 0;

    int iProcessedSrc = 0;
    int iProcessedDst = 0;
    int iChars = 0;

    for (;;) {
        unsigned int iUCS = 0;
        /* Convert symbol from UTF8 to UCS */
        iProcessedSrc = utf8tow(szSrc, iSrcLen, &iUCS);
        /* Incorrect input character */
        if (iProcessedSrc == -1) {
            free(szTMPDst);
            return -1;
        }

        szSrc += iProcessedSrc;
        iSrcLen -= iProcessedSrc;

        /* Modifies UTF-8. Special case for end-of-string */
        if (iSrcLen == 0 && iUCS == 0) {
            szTMPDst[iRealDstLen] = 0;
            break;
        }

        /* Change case */
        iUCS = ((*pCaseFn)(iUCS));

        /* Convert symbol from UCS to UTF8 */
        iProcessedDst = wtoutf8(iUCS, szTMPDst + iRealDstLen);
        /* Incorrect lowercase character; I hope, this should NEVER happened */
        if (iProcessedDst == -1) {
            free(szTMPDst);
            return -2;
        }
        /* Store processed size */
        iRealDstLen += iProcessedDst;

        /* Check free memory in destination buffer */
        if (iRealDstLen + 6 > iAllocated) {
            char * szReallocated = NULL;

            /* Reallocate buffer to 1.5 sizes of original */
            iAllocated = iAllocated + iAllocated / 2;
            szReallocated = realloc(szTMPDst, iAllocated * sizeof (char));
            /* Cannot reallocate memory */
            if (szReallocated == NULL) {
                free(szTMPDst);
                return -3;
            }
            szTMPDst = szReallocated;
        }

        /* Exit condition */
        if (iSrcLen == 0) {
            break;
        }


        ++iChars;
    }

    *iDstLen = iChars;
    *szDst = szTMPDst;
    return 0;
}

/*
 * Make a UTF8 string lowercase
 */
int utf8lcstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen) {
    return changeutf8case(szSrc, iSrcLen, szDst, iDstLen, wlc);
}


int utf8lcstr2(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen) {
    int ret = changeutf8case(szSrc, iSrcLen, szDst, iDstLen, wlc);
    if (ret < 0) {
        *szDst = (char*) malloc(sizeof (char) * iSrcLen);
        memcpy(*szDst,  szSrc, iSrcLen);
        *iDstLen = iSrcLen;
    }
    return 0;
}

int asciilcstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen) {
    int i;
    char * szTMPDst = (char*) malloc(sizeof (char) * iSrcLen);
    *iDstLen = iSrcLen;
    for (i = 0; i < iSrcLen; ++i) {
        if (szSrc[i] == '\0') {
             szTMPDst[i] = '\0';
             break;
        }
        szTMPDst[i] = (char) tolower(szSrc[i]);
    }
    *szDst = szTMPDst;
    return szTMPDst;
}

/*
 * Make a UTF8 string uppercase
 */
int utf8ucstr(const char * szSrc, int iSrcLen, char ** szDst, int * iDstLen) {
    return changeutf8case(szSrc, iSrcLen, szDst, iDstLen, wuc);
}

/* End. */
