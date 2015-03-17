/**************************************************************************************************
 *  EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2015 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#ifndef EJDBUTL_H
#define EJDBUTL_H

#include "tcutil.h"

EJDB_EXTERN_C_START


void ejdbqsort(void *a, size_t nel, size_t width, int (*compare) (const void *, const void *, void *opaque), void *opaque);

void ejdbqsortlist(TCLIST *list, int (*compar) (const TCLISTDATUM*, const TCLISTDATUM*, void *opaque), void *opaque);


EJDB_EXTERN_C_END

#endif        /* EJDBUTL_H */

