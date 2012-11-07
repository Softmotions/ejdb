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
 *      chartables.h
 *
 * $CAS$
 */
#ifndef _CHARTABLES_H__
#define _CHARTABLES_H__ 1

/*
  @file chartables.h
  @brief Character translation tables & accessors, header file

  Unicode uppercase to lowercase conversion mapping table
  http://publib.boulder.ibm.com/infocenter/systems/index.jsp?topic=/nls/rbagslowtoupmaptable.htm&tocNode=int_39939

  Unicode lowercase to uppercase conversion mapping table
  http://publib.boulder.ibm.com/infocenter/systems/index.jsp?topic=/nls/rbagslowtoupmaptable.htm&tocNode=int_39940
*/

/**
  @fn unsigned int wlc(unsigned int iChar);
  @brief Convert an upper-case letter to the corresponding lower-case letter.
  @param iChar - UCS character
  @return lowercase character if found in translation table or intouched character otherwise
*/
unsigned int wlc(unsigned int iChar);

/**
  @fn unsigned int wuc(unsigned int iChar)
  @brief Convert an lower-case letter to the corresponding upper-case letter.
  @param iChar - UCS character
  @return uppercase character if found in translation table or intouched character otherwise
*/
unsigned int wuc(unsigned int iChar);

#endif /* _CHARTABLES_H__ */
/* End. */
