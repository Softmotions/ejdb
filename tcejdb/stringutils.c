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

#include <stdio.h>
#include <stdlib.h>

/*
 * Find length of NULL-terminated UTF8 string
 */
int utf8strlen(const char * szSrc)
{
	int  iCharacters   = 0;
	int  iProcessedSrc = 0;

	for(;;)
	{
		unsigned int iUCS = 0;
		/* Exit condition */
		if (*szSrc == '\0') { break; }

		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc = utf8tow(szSrc, 6, &iUCS);

		/* Incorrect input character */
		if (iProcessedSrc == -1) { return -1; }

		szSrc   += iProcessedSrc;

		++iCharacters;
	}

return iCharacters;
}

/*
 * Find length of UTF8 string, check not more than iSrcLen bytes.
 */
int utf8strnlen(const char * szSrc, int iSrcLen)
{
	int  iCharacters   = 0;
	int  iProcessedSrc = 0;

	for(;;)
	{
		unsigned int iUCS = 0;
		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc = utf8tow(szSrc, iSrcLen, &iUCS);

		/* Incorrect input character */
		if (iProcessedSrc == -1) { return -1; }

		szSrc   += iProcessedSrc;
		iSrcLen -= iProcessedSrc;
		if (iUCS == 0 && iSrcLen == 0) { break; }

		++iCharacters;

		/* Exit condition */
		if (iSrcLen == 0) { break; }
	}

return iCharacters;
}

/*
 * Compare two utf-8 strings
 */
int utf8strncmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len)
{
	int     iProcessedSrc1 = 0;
	int     iProcessedSrc2 = 0;

	unsigned int iUCS1 = 0;
	unsigned int iUCS2 = 0;
	for(;;)
	{
		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc1 = utf8tow(szSrc1, iSrc1Len, &iUCS1);
		/* Incorrect input character  in first string */
		if (iProcessedSrc1 == -1) { return -2; }

		szSrc1   += iProcessedSrc1;
		iSrc1Len -= iProcessedSrc1;

		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc2 = utf8tow(szSrc2, iSrc2Len, &iUCS2);
		/* Incorrect input character in second string */
		if (iProcessedSrc2 == -1) { return 2; }

		szSrc2   += iProcessedSrc2;
		iSrc2Len -= iProcessedSrc2;

		/*
		 * In Modified UTF-8, the null character (U+0000) is encoded as 0xC0,0x80
		 * rather than 0x00, which is not valid UTF-8
		 */
		if (iUCS1    != iUCS2 ||
		    iUCS1    == 0     ||
		    iUCS2    == 0     ||
		    iSrc1Len == 0     ||
		    iSrc2Len == 0) { break; }
	}

	if (iUCS1 > iUCS2) { return  1; }
	if (iUCS1 < iUCS2) { return -1; }

return 0;
}

/*
 *Compare two utf-8 strings, ignoring case
 */
int utf8strncasecmp(const char * szSrc1, int iSrc1Len, const char * szSrc2, int iSrc2Len)
{
	int     iProcessedSrc1 = 0;
	int     iProcessedSrc2 = 0;

	unsigned int iUCS1 = 0;
	unsigned int iUCS2 = 0;
	for(;;)
	{
		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc1 = utf8tow(szSrc1, iSrc1Len, &iUCS1);
		/* Incorrect input character  in first string */
		if (iProcessedSrc1 == -1) { return -2; }

		szSrc1   += iProcessedSrc1;
		iSrc1Len -= iProcessedSrc1;

		/* Convert symbol from UTF8 to UCS */
		iProcessedSrc2 = utf8tow(szSrc2, iSrc2Len, &iUCS2);
		/* Incorrect input character in second string */
		if (iProcessedSrc2 == -1) { return 2; }

		szSrc2   += iProcessedSrc2;
		iSrc2Len -= iProcessedSrc2;

		/* Change case */
		iUCS1 = wlc(iUCS1);
		/* Change case */
		iUCS2 = wlc(iUCS2);

		/*
		 * In Modified UTF-8, the null character (U+0000) is encoded as 0xC0,0x80
		 * rather than 0x00, which is not valid UTF-8
		 */
		if (iUCS1    != iUCS2 ||
		    iUCS1    == 0     ||
		    iUCS2    == 0     ||
		    iSrc1Len == 0     ||
		    iSrc2Len == 0) { break; }
	}

	if (iUCS1 > iUCS2) { return  1; }
	if (iUCS1 < iUCS2) { return -1; }

return 0;
}

/* End. */
