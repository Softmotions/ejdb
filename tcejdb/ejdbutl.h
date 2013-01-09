/*
 * File:   ejdbutl.h
 * Author: adam
 *
 * Created on October 15, 2012, 1:26 PM
 */

#ifndef EJDBUTL_H
#define        EJDBUTL_H

#include "myconf.h"
#include "tcutil.h"

EJDB_EXTERN_C_START


void ejdbqsort(void *a, size_t nel, size_t width, int (*compare) (const void *, const void *, void *opaque), void *opaque);

void ejdbqsortlist(TCLIST *list, int (*compar) (const TCLISTDATUM*, const TCLISTDATUM*, void *opaque), void *opaque);


EJDB_EXTERN_C_END

#endif        /* EJDBUTL_H */

