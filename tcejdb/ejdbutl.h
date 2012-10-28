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

/**
 * A stable, adaptive, iterative mergesort that requires far fewer than
 * n lg(n) comparisons when running on partially sorted arrays, while
 * offering performance comparable to a traditional mergesort when run
 * on random arrays.  Like all proper mergesorts, this sort is stable and
 * runs O(n log n) time (worst case).  In the worst case, this sort requires
 * temporary storage space for n/2 object references; in the best case,
 * it requires only a small constant amount of space.
 *
 * This implementation was adapted from Josh Bloch's Java implementation of
 * Tim Peters's list sort for Python, which is described in detail here:
 *
 *   http://svn.python.org/projects/python/trunk/Objects/listsort.txt
 *
 * Tim's C code may be found here:
 *
 *   http://svn.python.org/projects/python/trunk/Objects/listobject.c
 *
 * Josh's (Apache 2.0 Licenced) Java code may be found here:
 *
 *   http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/src/main/java/util/TimSort.java?view=co
 *
 * The underlying techniques are described in this paper (and may have
 * even earlier origins):
 *
 *  "Optimistic Sorting and Information Theoretic Complexity"
 *  Peter McIlroy
 *  SODA (Fourth Annual ACM-SIAM Symposium on Discrete Algorithms),
 *  pp 467-474, Austin, Texas, 25-27 January 1993.
 *
 * While the API to this class consists solely of static methods, it is
 * (privately) instantiable; a TimSort instance holds the state of an ongoing
 * sort, assuming the input array is large enough to warrant the full-blown
 * TimSort. Small arrays are sorted in place, using a binary insertion sort.
 *
 * C implementation:
 *  https://github.com/patperry/timsort
 *
 *
 * @param a the array to be sorted
 * @param nel the length of the array
 * @param c the comparator to determine the order of the sort
 * @param width the element width
 * @param opaque data for the comparator function
 * @param opaque data for the comparator function
 *
 * @author Josh Bloch
 * @author Patrick O. Perry
 */

int ejdbtimsort(void *a, size_t nel, size_t width,
        int (*c) (const void *, const void *, void *opaque), void *opaque);

int ejdbtimsortlist(TCLIST *list,
        int (*compar) (const TCLISTDATUM*, const TCLISTDATUM*, void *opaque), void *opaque);


EJDB_EXTERN_C_END

#endif        /* EJDBUTL_H */

