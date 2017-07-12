/*************************************************************************************************
 * The utility API of Tokyo Cabinet
 *                                                               Copyright (C) 2006-2012 FAL Labs
 *                                                               Copyright (C) 2012-2015 Softmotions Ltd <info@softmotions.com>
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#include "tcutil.h"
#include "myconf.h"
#include "md5.h"

#ifdef _WIN32
#include <wincrypt.h>
#endif


/*************************************************************************************************
 * basic utilities
 *************************************************************************************************/

/* Call back function for handling a fatal error. */
void (*tcfatalfunc)(const char *message) = NULL;

/* Allocate a region on memory. */
void *tcmalloc(size_t size) {
    assert(size > 0 && size < INT_MAX);
    char *p;
    TCMALLOC(p, size);
    return p;
}

/* Allocate a nullified region on memory. */
void *tccalloc(size_t nmemb, size_t size) {
    assert(nmemb > 0 && nmemb < INT_MAX && size > 0 && size < INT_MAX);
    char *p;
    TCCALLOC(p, nmemb, size);
    return p;
}

/* Re-allocate a region on memory. */
void *tcrealloc(void *ptr, size_t size) {
    assert(size >= 0 && size < INT_MAX);
    char *p;
    TCREALLOC(p, ptr, size);
    return p;
}

/* Duplicate a region on memory. */
void *tcmemdup(const void *ptr, size_t size) {
    assert(ptr && size >= 0);
    char *p;
    TCMALLOC(p, size + 1);
    memcpy(p, ptr, size);
    p[size] = '\0';
    return p;
}

/* Duplicate a string on memory. */
char *tcstrdup(const void *str) {
    assert(str);
    int size = strlen(str);
    char *p;
    TCMALLOC(p, size + 1);
    memcpy(p, str, size);
    p[size] = '\0';
    return p;
}

/* Free a region on memory. */
void tcfree(void *ptr) {
    TCFREE(ptr);
}



/*************************************************************************************************
 * extensible string
 *************************************************************************************************/


#define TCXSTRUNIT     12                // allocation unit size of an extensible string


/* private function prototypes */
static void tcvxstrprintf(TCXSTR *xstr, const char *format, va_list ap);

/* Create an extensible string object. */
TCXSTR *tcxstrnew(void) {
    TCXSTR *xstr;
    TCMALLOC(xstr, sizeof (*xstr));
    TCMALLOC(xstr->ptr, TCXSTRUNIT);
    xstr->size = 0;
    xstr->asize = TCXSTRUNIT;
    xstr->ptr[0] = '\0';
    return xstr;
}

/* Create an extensible string object from a character string. */
TCXSTR *tcxstrnew2(const char *str) {
    assert(str);
    TCXSTR *xstr;
    TCMALLOC(xstr, sizeof (*xstr));
    int size = strlen(str);
    int asize = tclmax(size + 1, TCXSTRUNIT);
    TCMALLOC(xstr->ptr, asize);
    xstr->size = size;
    xstr->asize = asize;
    memcpy(xstr->ptr, str, size + 1);
    return xstr;
}

/* Create an extensible string object with the initial allocation size. */
TCXSTR *tcxstrnew3(int asiz) {
    assert(asiz >= 0);
    asiz = tclmax(asiz, TCXSTRUNIT);
    TCXSTR *xstr;
    TCMALLOC(xstr, sizeof (*xstr));
    TCMALLOC(xstr->ptr, asiz);
    xstr->size = 0;
    xstr->asize = asiz;
    xstr->ptr[0] = '\0';
    return xstr;
}

/* Copy an extensible string object. */
TCXSTR *tcxstrdup(const TCXSTR *xstr) {
    assert(xstr);
    TCXSTR *nxstr;
    TCMALLOC(nxstr, sizeof (*nxstr));
    int asize = tclmax(xstr->size + 1, TCXSTRUNIT);
    TCMALLOC(nxstr->ptr, asize);
    nxstr->size = xstr->size;
    nxstr->asize = asize;
    memcpy(nxstr->ptr, xstr->ptr, xstr->size + 1);
    return nxstr;
}

/* Delete an extensible string object. */
void tcxstrdel(TCXSTR *xstr) {
    assert(xstr);
    TCFREE(xstr->ptr);
    TCFREE(xstr);
}

/* Concatenate a region to the end of an extensible string object. */
void tcxstrcat(TCXSTR *xstr, const void *ptr, int size) {
    assert(xstr && ptr && size >= 0);
    int nsize = xstr->size + size + 1;
    if (xstr->asize < nsize) {
        while (xstr->asize < nsize) {
            xstr->asize *= 2;
            if (xstr->asize < nsize) xstr->asize = nsize;
        }
        TCREALLOC(xstr->ptr, xstr->ptr, xstr->asize);
    }
    memcpy(xstr->ptr + xstr->size, ptr, size);
    xstr->size += size;
    xstr->ptr[xstr->size] = '\0';
}

/* Concatenate a character string to the end of an extensible string object. */
void tcxstrcat2(TCXSTR *xstr, const char *str) {
    assert(xstr && str);
    int size = strlen(str);
    int nsize = xstr->size + size + 1;
    if (xstr->asize < nsize) {
        while (xstr->asize < nsize) {
            xstr->asize *= 2;
            if (xstr->asize < nsize) xstr->asize = nsize;
        }
        TCREALLOC(xstr->ptr, xstr->ptr, xstr->asize);
    }
    memcpy(xstr->ptr + xstr->size, str, size + 1);
    xstr->size += size;
}

/* Get the pointer of the region of an extensible string object. */
const void *tcxstrptr(const TCXSTR *xstr) {
    assert(xstr);
    return xstr->ptr;
}

/* Get the size of the region of an extensible string object. */
int tcxstrsize(const TCXSTR *xstr) {
    assert(xstr);
    return xstr->size;
}

/* Clear an extensible string object. */
void tcxstrclear(TCXSTR *xstr) {
    assert(xstr);
    xstr->size = 0;
    xstr->ptr[0] = '\0';
}

/* Perform formatted output into an extensible string object. */
void tcxstrprintf(TCXSTR *xstr, const char *format, ...) {
    assert(xstr && format);
    va_list ap;
    va_start(ap, format);
    tcvxstrprintf(xstr, format, ap);
    va_end(ap);
}

/* Allocate a formatted string on memory. */
char *tcsprintf(const char *format, ...) {
    assert(format);
    TCXSTR *xstr = tcxstrnew();
    va_list ap;
    va_start(ap, format);
    tcvxstrprintf(xstr, format, ap);
    va_end(ap);
    return tcxstrtomalloc(xstr);
}

/* Perform formatted output into an extensible string object. */
static void tcvxstrprintf(TCXSTR *xstr, const char *format, va_list ap) {
    assert(xstr && format);
    while (*format != '\0') {
        if (*format == '%') {
            char cbuf[TCNUMBUFSIZ];
            cbuf[0] = '%';
            int cblen = 1;
            int lnum = 0;
            format++;
            while (strchr("0123456789 .+-hlLzI", *format) && *format != '\0' &&
                    cblen < TCNUMBUFSIZ - 1) {
                if (*format == 'l' || *format == 'L') {
                    lnum++;
                } else if (*format == 'I' && *(format + 1) == '6' && *(format + 2) == '4') {
                    lnum += 2;
                }
                cbuf[cblen++] = *(format++);
            }
            cbuf[cblen++] = *format;
            cbuf[cblen] = '\0';
            int tlen;
            char *tmp, tbuf[TCNUMBUFSIZ * 4];
            switch (*format) {
                case 's':
                    tmp = va_arg(ap, char *);
                    if (!tmp) tmp = "(null)";
                    tcxstrcat2(xstr, tmp);
                    break;
                case 'd':
                    if (lnum >= 2) {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, long long));
                    } else if (lnum >= 1) {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, long));
                    } else {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, int));
                    }
                    TCXSTRCAT(xstr, tbuf, tlen);
                    break;
                case 'o': case 'u': case 'x': case 'X': case 'c':
                    if (lnum >= 2) {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned long long));
                    } else if (lnum >= 1) {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned long));
                    } else {
                        tlen = sprintf(tbuf, cbuf, va_arg(ap, unsigned int));
                    }
                    TCXSTRCAT(xstr, tbuf, tlen);
                    break;
                case 'e': case 'E': case 'f': case 'g': case 'G':
                    if (lnum > 1) {
                        //tlen = snprintf(tbuf, sizeof(tbuf), cbuf, va_arg(ap, long double))
                        tlen = tcftoa(va_arg(ap, long double), tbuf, sizeof (tbuf), 6);
                    } else {
                        //tlen = snprintf(tbuf, sizeof(tbuf), cbuf, va_arg(ap, double));
                        tlen = tcftoa(va_arg(ap, double), tbuf, sizeof (tbuf), 6);
                    }
                    if (tlen < 0 || tlen >= sizeof (tbuf)) {
                        tbuf[sizeof (tbuf) - 1] = '*';
                        tlen = sizeof (tbuf);
                    }
                    TCXSTRCAT(xstr, tbuf, tlen);
                    break;
                case '@':
                    tmp = va_arg(ap, char *);
                    if (!tmp) tmp = "(null)";
                    while (*tmp) {
                        switch (*tmp) {
                            case '&': TCXSTRCAT(xstr, "&amp;", 5);
                                break;
                            case '<': TCXSTRCAT(xstr, "&lt;", 4);
                                break;
                            case '>': TCXSTRCAT(xstr, "&gt;", 4);
                                break;
                            case '"': TCXSTRCAT(xstr, "&quot;", 6);
                                break;
                            default:
                                if (!((*tmp >= 0 && *tmp <= 0x8) || (*tmp >= 0x0e && *tmp <= 0x1f)))
                                    TCXSTRCAT(xstr, tmp, 1);
                                break;
                        }
                        tmp++;
                    }
                    break;
                case '?':
                    tmp = va_arg(ap, char *);
                    if (!tmp) tmp = "(null)";
                    while (*tmp) {
                        unsigned char c = *(unsigned char *) tmp;
                        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || (c != '\0' && strchr("_-.", c))) {
                            TCXSTRCAT(xstr, tmp, 1);
                        } else {
                            tlen = sprintf(tbuf, "%%%02X", c);
                            TCXSTRCAT(xstr, tbuf, tlen);
                        }
                        tmp++;
                    }
                    break;
                case 'b':
                    if (lnum >= 2) {
                        tlen = tcnumtostrbin(va_arg(ap, unsigned long long), tbuf,
                                tcatoi(cbuf + 1), (cbuf[1] == '0') ? '0' : ' ');
                    } else if (lnum >= 1) {
                        tlen = tcnumtostrbin(va_arg(ap, unsigned long), tbuf,
                                tcatoi(cbuf + 1), (cbuf[1] == '0') ? '0' : ' ');
                    } else {
                        tlen = tcnumtostrbin(va_arg(ap, unsigned int), tbuf,
                                tcatoi(cbuf + 1), (cbuf[1] == '0') ? '0' : ' ');
                    }
                    TCXSTRCAT(xstr, tbuf, tlen);
                    break;
                case '%':
                    TCXSTRCAT(xstr, "%", 1);
                    break;
            }
        } else {
            TCXSTRCAT(xstr, format, 1);
        }
        format++;
    }
}



/*************************************************************************************************
 * extensible string (for experts)
 *************************************************************************************************/

/* Convert an extensible string object into a usual allocated region. */
void *tcxstrtomalloc(TCXSTR *xstr) {
    assert(xstr);
    char *ptr;
    ptr = xstr->ptr;
    TCFREE(xstr);
    return ptr;
}

/* Create an extensible string object from an allocated region. */
TCXSTR *tcxstrfrommalloc(void *ptr, int size) {
    TCXSTR *xstr;
    TCMALLOC(xstr, sizeof (*xstr));
    TCREALLOC(xstr->ptr, ptr, size + 1);
    xstr->ptr[size] = '\0';
    xstr->size = size;
    xstr->asize = size;
    return xstr;
}



/*************************************************************************************************
 * array list
 *************************************************************************************************/


#define TCLISTUNIT     64                // allocation unit number of a list handle


/* private function prototypes */
static int tclistelemcmp(const void *a, const void *b);
static int tclistelemcmpci(const void *a, const void *b);

/* Create a list object. */
TCLIST *tclistnew(void) {
    TCLIST *list;
    TCMALLOC(list, sizeof (*list));
    list->anum = TCLISTUNIT;
    TCMALLOC(list->array, sizeof (list->array[0]) * list->anum);
    list->start = 0;
    list->num = 0;
    return list;
}

/* Create a list object. */
TCLIST *tclistnew2(int anum) {
    TCLIST *list;
    TCMALLOC(list, sizeof (*list));
    if (anum < 1) anum = 1;
    list->anum = anum;
    TCMALLOC(list->array, sizeof (list->array[0]) * list->anum);
    list->start = 0;
    list->num = 0;
    return list;
}

/* Create a list object with initial string elements. */
TCLIST *tclistnew3(const char *str, ...) {
    TCLIST *list = tclistnew();
    if (str) {
        tclistpush2(list, str);
        va_list ap;
        va_start(ap, str);
        const char *elem;
        while ((elem = va_arg(ap, char *)) != NULL) {
            tclistpush2(list, elem);
        }
        va_end(ap);
    }
    return list;
}

/* Copy a list object. */
TCLIST *tclistdup(const TCLIST *list) {
    assert(list);
    int num = list->num;
    if (num < 1) return tclistnew();
    const TCLISTDATUM *array = list->array + list->start;
    TCLIST *nlist;
    TCMALLOC(nlist, sizeof (*nlist));
    TCLISTDATUM *narray;
    TCMALLOC(narray, sizeof (list->array[0]) * num);
    for (int i = 0; i < num; i++) {
        int size = array[i].size;
        TCMALLOC(narray[i].ptr, tclmax(size + 1, TCXSTRUNIT));
        memcpy(narray[i].ptr, array[i].ptr, size + 1);
        narray[i].size = array[i].size;
    }
    nlist->anum = num;
    nlist->array = narray;
    nlist->start = 0;
    nlist->num = num;
    return nlist;
}

/* Delete a list object. */
void tclistdel(TCLIST *list) {
    assert(list);
    TCLISTDATUM *array = list->array;
    int end = list->start + list->num;
    for (int i = list->start; i < end; i++) {
        TCFREE(array[i].ptr);
    }
    TCFREE(list->array);
    TCFREE(list);
}

/* Get the number of elements of a list object. */
int tclistnum(const TCLIST *list) {
    assert(list);
    return list->num;
}

/* Get the pointer to the region of an element of a list object. */
const void *tclistval(const TCLIST *list, int index, int *sp) {
    assert(list && index >= 0 && sp);
    if (index >= list->num) {
		*sp = 0;
		return NULL;
	}
    index += list->start;
    *sp = list->array[index].size;
    return list->array[index].ptr;
}

/* Get the string of an element of a list object. */
const char *tclistval2(const TCLIST *list, int index) {
    assert(list && index >= 0);
    if (index >= list->num) return NULL;
    index += list->start;
    return list->array[index].ptr;
}

/* Add an element at the end of a list object. */
void tclistpush(TCLIST *list, const void *ptr, int size) {
    assert(list && ptr && size >= 0);
    int index = list->start + list->num;
    if (index >= list->anum) {
        list->anum += list->num + 1;
        TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
    }
    TCLISTDATUM *array = list->array;
    TCMALLOC(array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(array[index].ptr, ptr, size);
    array[index].ptr[size] = '\0';
    array[index].size = size;
    list->num++;
}

/* Add a string element at the end of a list object. */
void tclistpush2(TCLIST *list, const char *str) {
    assert(list && str);
    int index = list->start + list->num;
    if (index >= list->anum) {
        list->anum += list->num + 1;
        TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
    }
    int size = strlen(str);
    TCLISTDATUM *array = list->array;
    TCMALLOC(array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(array[index].ptr, str, size + 1);
    array[index].size = size;
    list->num++;
}

/* Remove an element of the end of a list object. */
void *tclistpop(TCLIST *list, int *sp) {
    assert(list && sp);
    if (list->num < 1) return NULL;
    int index = list->start + list->num - 1;
    list->num--;
    *sp = list->array[index].size;
    return list->array[index].ptr;
}

/* Remove a string element of the end of a list object. */
char *tclistpop2(TCLIST *list) {
    assert(list);
    if (list->num < 1) return NULL;
    int index = list->start + list->num - 1;
    list->num--;
    return list->array[index].ptr;
}

/* Add an element at the top of a list object. */
void tclistunshift(TCLIST *list, const void *ptr, int size) {
    assert(list && ptr && size >= 0);
    if (list->start < 1) {
        if (list->start + list->num >= list->anum) {
            list->anum += list->num + 1;
            TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
        }
        list->start = list->anum - list->num;
        memmove(list->array + list->start, list->array, list->num * sizeof (list->array[0]));
    }
    int index = list->start - 1;
    TCMALLOC(list->array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(list->array[index].ptr, ptr, size);
    list->array[index].ptr[size] = '\0';
    list->array[index].size = size;
    list->start--;
    list->num++;
}

/* Add a string element at the top of a list object. */
void tclistunshift2(TCLIST *list, const char *str) {
    assert(list && str);
    if (list->start < 1) {
        if (list->start + list->num >= list->anum) {
            list->anum += list->num + 1;
            TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
        }
        list->start = list->anum - list->num;
        memmove(list->array + list->start, list->array, list->num * sizeof (list->array[0]));
    }
    int index = list->start - 1;
    int size = strlen(str);
    TCMALLOC(list->array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(list->array[index].ptr, str, size + 1);
    list->array[index].size = size;
    list->start--;
    list->num++;
}

/* Remove an element of the top of a list object. */
void *tclistshift(TCLIST *list, int *sp) {
    assert(list && sp);
    if (list->num < 1) return NULL;
    int index = list->start;
    list->start++;
    list->num--;
    *sp = list->array[index].size;
    void *rv = list->array[index].ptr;
    if ((list->start & 0xff) == 0 && list->start > (list->num >> 1)) {
        memmove(list->array, list->array + list->start, list->num * sizeof (list->array[0]));
        list->start = 0;
    }
    return rv;
}

/* Remove a string element of the top of a list object. */
char *tclistshift2(TCLIST *list) {
    assert(list);
    if (list->num < 1) return NULL;
    int index = list->start;
    list->start++;
    list->num--;
    void *rv = list->array[index].ptr;
    if ((list->start & 0xff) == 0 && list->start > (list->num >> 1)) {
        memmove(list->array, list->array + list->start, list->num * sizeof (list->array[0]));
        list->start = 0;
    }
    return rv;
}

/* Add an element at the specified location of a list object. */
void tclistinsert(TCLIST *list, int index, const void *ptr, int size) {
    assert(list && index >= 0 && ptr && size >= 0);
    if (index > list->num) return;
    index += list->start;
    if (list->start + list->num >= list->anum) {
        list->anum += list->num + 1;
        TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
    }
    memmove(list->array + index + 1, list->array + index,
            sizeof (list->array[0]) * (list->start + list->num - index));
    TCMALLOC(list->array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(list->array[index].ptr, ptr, size);
    list->array[index].ptr[size] = '\0';
    list->array[index].size = size;
    list->num++;
}

/* Add a string element at the specified location of a list object. */
void tclistinsert2(TCLIST *list, int index, const char *str) {
    assert(list && index >= 0 && str);
    if (index > list->num) return;
    index += list->start;
    if (list->start + list->num >= list->anum) {
        list->anum += list->num + 1;
        TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
    }
    memmove(list->array + index + 1, list->array + index,
            sizeof (list->array[0]) * (list->start + list->num - index));
    int size = strlen(str);
    TCMALLOC(list->array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
    memcpy(list->array[index].ptr, str, size);
    list->array[index].ptr[size] = '\0';
    list->array[index].size = size;
    list->num++;
}

/* Remove an element at the specified location of a list object. */
void *tclistremove(TCLIST *list, int index, int *sp) {
    assert(list && index >= 0 && sp);
    if (index >= list->num) return NULL;
    index += list->start;
    void *rv = list->array[index].ptr;
    *sp = list->array[index].size;
    list->num--;
    memmove(list->array + index, list->array + index + 1,
            sizeof (list->array[0]) * (list->start + list->num - index));
    return rv;
}

/* Remove a string element at the specified location of a list object. */
char *tclistremove2(TCLIST *list, int index) {
    assert(list && index >= 0);
    if (index >= list->num) return NULL;
    index += list->start;
    void *rv = list->array[index].ptr;
    list->num--;
    memmove(list->array + index, list->array + index + 1,
            sizeof (list->array[0]) * (list->start + list->num - index));
    return rv;
}

/* Overwrite an element at the specified location of a list object. */
void tclistover(TCLIST *list, int index, const void *ptr, int size) {
    assert(list && index >= 0 && ptr && size >= 0);
    if (index >= list->num) return;
    index += list->start;
    if (size > list->array[index].size)
        TCREALLOC(list->array[index].ptr, list->array[index].ptr, size + 1);
    memcpy(list->array[index].ptr, ptr, size);
    list->array[index].size = size;
    list->array[index].ptr[size] = '\0';
}

/* Overwrite a string element at the specified location of a list object. */
void tclistover2(TCLIST *list, int index, const char *str) {
    assert(list && index >= 0 && str);
    if (index >= list->num) return;
    index += list->start;
    int size = strlen(str);
    if (size > list->array[index].size)
        TCREALLOC(list->array[index].ptr, list->array[index].ptr, size + 1);
    memcpy(list->array[index].ptr, str, size + 1);
    list->array[index].size = size;
}

/* Sort elements of a list object in lexical order. */
void tclistsort(TCLIST *list) {
    assert(list);
    qsort(list->array + list->start, list->num, sizeof (list->array[0]), tclistelemcmp);
}

/* Search a list object for an element using liner search. */
int tclistlsearch(const TCLIST *list, const void *ptr, int size) {
    assert(list && ptr && size >= 0);
    int end = list->start + list->num;
    for (int i = list->start; i < end; i++) {
        if (list->array[i].size == size && !memcmp(list->array[i].ptr, ptr, size))
            return i - list->start;
    }
    return -1;
}

/* Search a list object for an element using binary search. */
int tclistbsearch(const TCLIST *list, const void *ptr, int size) {
    assert(list && ptr && size >= 0);
    TCLISTDATUM key;
    key.ptr = (char *) ptr;
    key.size = size;
    TCLISTDATUM *res = bsearch(&key, list->array + list->start,
            list->num, sizeof (list->array[0]), tclistelemcmp);
    return res ? res - list->array - list->start : -1;
}

/* Clear a list object. */
void tclistclear(TCLIST *list) {
    assert(list);
    TCLISTDATUM *array = list->array;
    int end = list->start + list->num;
    for (int i = list->start; i < end; i++) {
        TCFREE(array[i].ptr);
    }
    list->start = 0;
    list->num = 0;
}

/* Serialize a list object into a byte array. */
void *tclistdump(const TCLIST *list, int *sp) {
    assert(list && sp);
    const TCLISTDATUM *array = list->array;
    int end = list->start + list->num;
    int tsiz = 0;
    for (int i = list->start; i < end; i++) {
        tsiz += array[i].size + sizeof (int);
    }
    char *buf;
    TCMALLOC(buf, tsiz + 1);
    char *wp = buf;
    for (int i = list->start; i < end; i++) {
        int step;
        TCSETVNUMBUF(step, wp, array[i].size);
        wp += step;
        memcpy(wp, array[i].ptr, array[i].size);
        wp += array[i].size;
    }
    *sp = wp - buf;
    return buf;
}

/* Create a list object from a serialized byte array. */
TCLIST *tclistload(const void *ptr, int size) {
    assert(ptr && size >= 0);
    TCLIST *list;
    TCMALLOC(list, sizeof (*list));
    int anum = size / sizeof (int) + 1;
    TCLISTDATUM *array;
    TCMALLOC(array, sizeof (array[0]) * anum);
    int num = 0;
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, vsiz;
        TCREADVNUMBUF(rp, vsiz, step);
        rp += step;
        if (num >= anum) {
            anum *= 2;
            TCREALLOC(array, array, anum * sizeof (array[0]));
        }
        TCMALLOC(array[num].ptr, tclmax(vsiz + 1, TCXSTRUNIT));
        memcpy(array[num].ptr, rp, vsiz);
        array[num].ptr[vsiz] = '\0';
        array[num].size = vsiz;
        num++;
        rp += vsiz;
    }
    list->anum = anum;
    list->array = array;
    list->start = 0;
    list->num = num;
    return list;
}

/* Compare two list elements in lexical order.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tclistelemcmp(const void *a, const void *b) {
    assert(a && b);
    unsigned char *ao = (unsigned char *) ((TCLISTDATUM *) a)->ptr;
    unsigned char *bo = (unsigned char *) ((TCLISTDATUM *) b)->ptr;
    int size = (((TCLISTDATUM *) a)->size < ((TCLISTDATUM *) b)->size) ?
            ((TCLISTDATUM *) a)->size : ((TCLISTDATUM *) b)->size;
    for (int i = 0; i < size; i++) {
        if (ao[i] > bo[i]) return 1;
        if (ao[i] < bo[i]) return -1;
    }
    return ((TCLISTDATUM *) a)->size - ((TCLISTDATUM *) b)->size;
}

/* Compare two list elements in case-insensitive lexical order..
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tclistelemcmpci(const void *a, const void *b) {
    assert(a && b);
    TCLISTDATUM *ap = (TCLISTDATUM *) a;
    TCLISTDATUM *bp = (TCLISTDATUM *) b;
    unsigned char *ao = (unsigned char *) ap->ptr;
    unsigned char *bo = (unsigned char *) bp->ptr;
    int size = (ap->size < bp->size) ? ap->size : bp->size;
    for (int i = 0; i < size; i++) {
        int ac = ao[i];
        bool ab = false;
        if (ac >= 'A' && ac <= 'Z') {
            ac += 'a' - 'A';
            ab = true;
        }
        int bc = bo[i];
        bool bb = false;
        if (bc >= 'A' && bc <= 'Z') {
            bc += 'a' - 'A';
            bb = true;
        }
        if (ac > bc) return 1;
        if (ac < bc) return -1;
        if (!ab && bb) return 1;
        if (ab && !bb) return -1;
    }
    return ap->size - bp->size;
}



/*************************************************************************************************
 * array list (for experts)
 *************************************************************************************************/

/* Add an allocated element at the end of a list object. */
void tclistpushmalloc(TCLIST *list, void *ptr, int size) {
    assert(list && ptr && size >= 0);
    int index = list->start + list->num;
    if (index >= list->anum) {
        list->anum += list->num + 1;
        TCREALLOC(list->array, list->array, list->anum * sizeof (list->array[0]));
    }
    TCLISTDATUM *array = list->array;
    TCREALLOC(array[index].ptr, ptr, size + 1);
    array[index].ptr[size] = '\0';
    array[index].size = size;
    list->num++;
}

/* Sort elements of a list object in case-insensitive lexical order. */
void tclistsortci(TCLIST *list) {
    assert(list);
    qsort(list->array + list->start, list->num, sizeof (list->array[0]), tclistelemcmpci);
}

/* Sort elements of a list object by an arbitrary comparison function. */
void tclistsortex(TCLIST *list, int (*cmp)(const TCLISTDATUM *, const TCLISTDATUM *)) {
    assert(list && cmp);
    qsort(list->array + list->start, list->num, sizeof (list->array[0]),
            (int (*)(const void *, const void *))cmp);
}

/* Invert elements of a list object. */
void tclistinvert(TCLIST *list) {
    assert(list);
    TCLISTDATUM *top = list->array + list->start;
    TCLISTDATUM *bot = top + list->num - 1;
    while (top < bot) {
        TCLISTDATUM swap = *top;
        *top = *bot;
        *bot = swap;
        top++;
        bot--;
    }
}

/* Perform formatted output into a list object. */
void tclistprintf(TCLIST *list, const char *format, ...) {
    assert(list && format);
    TCXSTR *xstr = tcxstrnew();
    va_list ap;
    va_start(ap, format);
    tcvxstrprintf(xstr, format, ap);
    va_end(ap);
    int size = TCXSTRSIZE(xstr);
    char *ptr = tcxstrtomalloc(xstr);
    tclistpushmalloc(list, ptr, size);
}



/*************************************************************************************************
 * hash map
 *************************************************************************************************/


#define TCMAPKMAXSIZ   0xfffff           // maximum size of each key
#define TCMAPDEFBNUM   4093              // default number of buckets
#define TCMAPZMMINSIZ  131072            // minimum memory size to use nullified region
#define TCMAPCSUNIT    52                // small allocation unit size of map concatenation
#define TCMAPCBUNIT    252               // big allocation unit size of map concatenation

/* get the first hash value */
#define TCMAPHASH1(TC_res, TC_kbuf, TC_ksiz)                            \
  do {                                                                  \
    const unsigned char *_TC_p = (const unsigned char *)(TC_kbuf);      \
    int _TC_ksiz = TC_ksiz;                                             \
    for((TC_res) = 19780211; _TC_ksiz--;){                              \
      (TC_res) = (TC_res) * 37 + *(_TC_p)++;                            \
    }                                                                   \
  } while(false)

/* get the second hash value */
#define TCMAPHASH2(TC_res, TC_kbuf, TC_ksiz)                            \
  do {                                                                  \
    const unsigned char *_TC_p = (const unsigned char *)(TC_kbuf) + TC_ksiz - 1; \
    int _TC_ksiz = TC_ksiz;                                             \
    for((TC_res) = 0x13579bdf; _TC_ksiz--;){                            \
      (TC_res) = (TC_res) * 31 + *(_TC_p)--;                            \
    }                                                                   \
  } while(false)

/* compare two keys */
#define TCKEYCMP(TC_abuf, TC_asiz, TC_bbuf, TC_bsiz)                    \
  ((TC_asiz > TC_bsiz) ? 1 : (TC_asiz < TC_bsiz) ? -1 : memcmp(TC_abuf, TC_bbuf, TC_asiz))

/* Create a map object. */
TCMAP *tcmapnew(void) {
    return tcmapnew2(TCMAPDEFBNUM);
}

/* Create a map object with specifying the number of the buckets. */
TCMAP *tcmapnew2(uint32_t bnum) {
    if (bnum < 1) bnum = 1;
    TCMAP *map;
    TCMALLOC(map, sizeof (*map));
    TCMAPREC **buckets;
    if (bnum >= TCMAPZMMINSIZ / sizeof (*buckets)) {
        buckets = tczeromap(bnum * sizeof (*buckets));
    } else {
        TCCALLOC(buckets, bnum, sizeof (*buckets));
    }
    map->buckets = buckets;
    map->first = NULL;
    map->last = NULL;
    map->cur = NULL;
    map->bnum = bnum;
    map->rnum = 0;
    map->msiz = 0;
    return map;
}

/* Create a map object with initial string elements. */
TCMAP *tcmapnew3(const char *str, ...) {
    TCMAP *map = tcmapnew2(TCMAPTINYBNUM);
    if (str) {
        va_list ap;
        va_start(ap, str);
        const char *key = str;
        const char *elem;
        while ((elem = va_arg(ap, char *)) != NULL) {
            if (key) {
                tcmapput2(map, key, elem);
                key = NULL;
            } else {
                key = elem;
            }
        }
        va_end(ap);
    }
    return map;
}

/* Copy a map object. */
TCMAP *tcmapdup(const TCMAP *map) {
    assert(map);
    TCMAP *nmap = tcmapnew2(tclmax(tclmax(map->bnum, map->rnum), TCMAPDEFBNUM));
    TCMAPREC *rec = map->first;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        tcmapput(nmap, dbuf, rksiz, dbuf + rksiz + TCALIGNPAD(rksiz), rec->vsiz);
        rec = rec->next;
    }
    return nmap;
}

/* Close a map object. */
void tcmapdel(TCMAP *map) {
    assert(map);
    TCMAPREC *rec = map->first;
    while (rec) {
        TCMAPREC *next = rec->next;
        TCFREE(rec);
        rec = next;
    }
    if (map->bnum >= TCMAPZMMINSIZ / sizeof (map->buckets[0])) {
        tczerounmap(map->buckets);
    } else {
        TCFREE(map->buckets);
    }
    TCFREE(map);
}

/* Store a record into a map object. */
void tcmapput(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                map->msiz += vsiz - rec->vsiz;
                int psiz = TCALIGNPAD(ksiz);
                if (vsiz > rec->vsiz) {
                    TCMAPREC *old = rec;
                    TCREALLOC(rec, rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
                    if (rec != old) {
                        if (map->first == old) map->first = rec;
                        if (map->last == old) map->last = rec;
                        if (map->cur == old) map->cur = rec;
                        *entp = rec;
                        if (rec->prev) rec->prev->next = rec;
                        if (rec->next) rec->next->prev = rec;
                        dbuf = (char *) rec + sizeof (*rec);
                    }
                }
                memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
                dbuf[ksiz + psiz + vsiz] = '\0';
                rec->vsiz = vsiz;
                return;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
}

/* Store a string record into a map object. */
void tcmapput2(TCMAP *map, const char *kstr, const char *vstr) {
    assert(map && kstr && vstr);
    tcmapput(map, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a new record into a map object. */
bool tcmapputkeep(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                return false;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
    return true;
}

/* Store a new string record into a map object. */
bool tcmapputkeep2(TCMAP *map, const char *kstr, const char *vstr) {
    assert(map && kstr && vstr);
    return tcmapputkeep(map, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Concatenate a value at the end of the value of the existing record in a map object. */
void tcmapputcat(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                map->msiz += vsiz;
                int psiz = TCALIGNPAD(ksiz);
                int asiz = sizeof (*rec) + ksiz + psiz + rec->vsiz + vsiz + 1;
                int unit = (asiz <= TCMAPCSUNIT) ? TCMAPCSUNIT : TCMAPCBUNIT;
                asiz = (asiz - 1) + unit - (asiz - 1) % unit;
                TCMAPREC *old = rec;
                TCREALLOC(rec, rec, asiz);
                if (rec != old) {
                    if (map->first == old) map->first = rec;
                    if (map->last == old) map->last = rec;
                    if (map->cur == old) map->cur = rec;
                    *entp = rec;
                    if (rec->prev) rec->prev->next = rec;
                    if (rec->next) rec->next->prev = rec;
                    dbuf = (char *) rec + sizeof (*rec);
                }
                memcpy(dbuf + ksiz + psiz + rec->vsiz, vbuf, vsiz);
                rec->vsiz += vsiz;
                dbuf[ksiz + psiz + rec->vsiz] = '\0';
                return;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    int asiz = sizeof (*rec) + ksiz + psiz + vsiz + 1;
    int unit = (asiz <= TCMAPCSUNIT) ? TCMAPCSUNIT : TCMAPCBUNIT;
    asiz = (asiz - 1) + unit - (asiz - 1) % unit;
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, asiz);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
}

/* Concatenate a string value at the end of the value of the existing record in a map object. */
void tcmapputcat2(TCMAP *map, const char *kstr, const char *vstr) {
    assert(map && kstr && vstr);
    tcmapputcat(map, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Remove a record of a map object. */
bool tcmapout(TCMAP *map, const void *kbuf, int ksiz) {
    assert(map && kbuf && ksiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                map->rnum--;
                map->msiz -= rksiz + rec->vsiz;
                if (rec->prev) rec->prev->next = rec->next;
                if (rec->next) rec->next->prev = rec->prev;
                if (rec == map->first) map->first = rec->next;
                if (rec == map->last) map->last = rec->prev;
                if (rec == map->cur) map->cur = rec->next;
                if (rec->left && !rec->right) {
                    *entp = rec->left;
                } else if (!rec->left && rec->right) {
                    *entp = rec->right;
                } else if (!rec->left) {
                    *entp = NULL;
                } else {
                    *entp = rec->left;
                    TCMAPREC *tmp = *entp;
                    while (tmp->right) {
                        tmp = tmp->right;
                    }
                    tmp->right = rec->right;
                }
                TCFREE(rec);
                return true;
            }
        }
    }
    return false;
}

/* Remove a string record of a map object. */
bool tcmapout2(TCMAP *map, const char *kstr) {
    assert(map && kstr);
    return tcmapout(map, kstr, strlen(kstr));
}

/* Retrieve a record in a map object. */
const void *tcmapget(const TCMAP *map, const void *kbuf, int ksiz, int *sp) {
    assert(map && kbuf && ksiz >= 0 && sp);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    TCMAPREC *rec = map->buckets[hash % map->bnum];
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            rec = rec->left;
        } else if (hash < rhash) {
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                rec = rec->left;
            } else if (kcmp > 0) {
                rec = rec->right;
            } else {
                *sp = rec->vsiz;
                return dbuf + rksiz + TCALIGNPAD(rksiz);
            }
        }
    }
    return NULL;
}

/* Retrieve a string record in a map object. */
const char *tcmapget2(const TCMAP *map, const char *kstr) {
    assert(map && kstr);
    int ksiz = strlen(kstr);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kstr, ksiz);
    TCMAPREC *rec = map->buckets[hash % map->bnum];
    TCMAPHASH2(hash, kstr, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            rec = rec->left;
        } else if (hash < rhash) {
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kstr, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                rec = rec->left;
            } else if (kcmp > 0) {
                rec = rec->right;
            } else {
                return dbuf + rksiz + TCALIGNPAD(rksiz);
            }
        }
    }
    return NULL;
}

/* Move a record to the edge of a map object. */
bool tcmapmove(TCMAP *map, const void *kbuf, int ksiz, bool head) {
    assert(map && kbuf && ksiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    TCMAPREC *rec = map->buckets[hash % map->bnum];
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            rec = rec->left;
        } else if (hash < rhash) {
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                rec = rec->left;
            } else if (kcmp > 0) {
                rec = rec->right;
            } else {
                if (head) {
                    if (map->first == rec) return true;
                    if (map->last == rec) map->last = rec->prev;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    rec->prev = NULL;
                    rec->next = map->first;
                    map->first->prev = rec;
                    map->first = rec;
                } else {
                    if (map->last == rec) return true;
                    if (map->first == rec) map->first = rec->next;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    rec->prev = map->last;
                    rec->next = NULL;
                    map->last->next = rec;
                    map->last = rec;
                }
                return true;
            }
        }
    }
    return false;
}

/* Move a string record to the edge of a map object. */
bool tcmapmove2(TCMAP *map, const char *kstr, bool head) {
    assert(map && kstr);
    return tcmapmove(map, kstr, strlen(kstr), head);
}

/* Initialize the iterator of a map object. */
void tcmapiterinit(TCMAP *map) {
    assert(map);
    map->cur = map->first;
}

/* Get the next key of the iterator of a map object. */
const void *tcmapiternext(TCMAP *map, int *sp) {
    assert(map && sp);
    TCMAPREC *rec;
    if (!map->cur) return NULL;
    rec = map->cur;
    map->cur = rec->next;
    *sp = rec->ksiz & TCMAPKMAXSIZ;
    return (char *) rec + sizeof (*rec);
}

/* Get the next key string of the iterator of a map object. */
const char *tcmapiternext2(TCMAP *map) {
    assert(map);
    TCMAPREC *rec;
    if (!map->cur) return NULL;
    rec = map->cur;
    map->cur = rec->next;
    return (char *) rec + sizeof (*rec);
}

/* Get the number of records stored in a map object. */
uint64_t tcmaprnum(const TCMAP *map) {
    assert(map);
    return map->rnum;
}

/* Get the total size of memory used in a map object. */
uint64_t tcmapmsiz(const TCMAP *map) {
    assert(map);
    return map->msiz + map->rnum * (sizeof (*map->first) + sizeof (tcgeneric_t)) +
            map->bnum * sizeof (void *);
}

/* Create a list object containing all keys in a map object. */
TCLIST *tcmapkeys(const TCMAP *map) {
    assert(map);
    TCLIST *list = tclistnew2(map->rnum);
    TCMAPREC *rec = map->first;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        TCLISTPUSH(list, dbuf, rec->ksiz & TCMAPKMAXSIZ);
        rec = rec->next;
    }
    return list;
}

/* Create a list object containing all values in a map object. */
TCLIST *tcmapvals(const TCMAP *map) {
    assert(map);
    TCLIST *list = tclistnew2(map->rnum);
    TCMAPREC *rec = map->first;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        TCLISTPUSH(list, dbuf + rksiz + TCALIGNPAD(rksiz), rec->vsiz);
        rec = rec->next;
    }
    return list;
}

/* Add an integer to a record in a map object. */
int tcmapaddint(TCMAP *map, const void *kbuf, int ksiz, int num) {
    assert(map && kbuf && ksiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                if (rec->vsiz != sizeof (num)) return INT_MIN;
                int *resp = (int *) (dbuf + ksiz + TCALIGNPAD(ksiz));
                return *resp += num;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
    dbuf[ksiz + psiz + sizeof (num)] = '\0';
    rec->vsiz = sizeof (num);
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
    return num;
}

/* Add a real number to a record in a map object. */
double tcmapadddouble(TCMAP *map, const void *kbuf, int ksiz, double num) {
    assert(map && kbuf && ksiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                if (rec->vsiz != sizeof (num)) return nan("");
                double *resp = (double *) (dbuf + ksiz + TCALIGNPAD(ksiz));
                return *resp += num;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
    dbuf[ksiz + psiz + sizeof (num)] = '\0';
    rec->vsiz = sizeof (num);
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
    return num;
}

/* Clear a map object. */
void tcmapclear(TCMAP *map) {
    assert(map);
    TCMAPREC *rec = map->first;
    while (rec) {
        TCMAPREC *next = rec->next;
        TCFREE(rec);
        rec = next;
    }
    TCMAPREC **buckets = map->buckets;
    int bnum = map->bnum;
    for (int i = 0; i < bnum; i++) {
        buckets[i] = NULL;
    }
    map->first = NULL;
    map->last = NULL;
    map->cur = NULL;
    map->rnum = 0;
    map->msiz = 0;
}

/* Remove front records of a map object. */
void tcmapcutfront(TCMAP *map, int num) {
    assert(map && num >= 0);
    tcmapiterinit(map);
    while (num-- > 0) {
        int ksiz;
        const char *kbuf = tcmapiternext(map, &ksiz);
        if (!kbuf) break;
        tcmapout(map, kbuf, ksiz);
    }
}

/* Serialize a map object into a byte array. */
void *tcmapdump(const TCMAP *map, int *sp) {
    assert(map && sp);
    int tsiz = 0;
    TCMAPREC *rec = map->first;
    while (rec) {
        tsiz += (rec->ksiz & TCMAPKMAXSIZ) + rec->vsiz + sizeof (int) * 2;
        rec = rec->next;
    }
    char *buf;
    TCMALLOC(buf, tsiz + 1);
    char *wp = buf;
    rec = map->first;
    while (rec) {
        const char *kbuf = (char *) rec + sizeof (*rec);
        int ksiz = rec->ksiz & TCMAPKMAXSIZ;
        const char *vbuf = kbuf + ksiz + TCALIGNPAD(ksiz);
        int vsiz = rec->vsiz;
        int step;
        TCSETVNUMBUF(step, wp, ksiz);
        wp += step;
        memcpy(wp, kbuf, ksiz);
        wp += ksiz;
        TCSETVNUMBUF(step, wp, vsiz);
        wp += step;
        memcpy(wp, vbuf, vsiz);
        wp += vsiz;
        rec = rec->next;
    }
    *sp = wp - buf;
    return buf;
}

/* Create a map object from a serialized byte array. */
TCMAP *tcmapload(const void *ptr, int size) {
    assert(ptr && size >= 0);
    TCMAP *map = tcmapnew2(tclmin(size / 6 + 1, TCMAPDEFBNUM));
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, ksiz, vsiz;
        TCREADVNUMBUF(rp, ksiz, step);
        rp += step;
        const char *kbuf = rp;
        rp += ksiz;
        TCREADVNUMBUF(rp, vsiz, step);
        rp += step;
        tcmapputkeep(map, kbuf, ksiz, rp, vsiz);
        rp += vsiz;
    }
    return map;
}



/*************************************************************************************************
 * hash map (for experts)
 *************************************************************************************************/

/* Store a record and make it semivolatile in a map object. */
void tcmapput3(TCMAP *map, const void *kbuf, int ksiz, const char *vbuf, int vsiz) {
    assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                map->msiz += vsiz - rec->vsiz;
                int psiz = TCALIGNPAD(ksiz);
                if (vsiz > rec->vsiz) {
                    TCMAPREC *old = rec;
                    TCREALLOC(rec, rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
                    if (rec != old) {
                        if (map->first == old) map->first = rec;
                        if (map->last == old) map->last = rec;
                        if (map->cur == old) map->cur = rec;
                        *entp = rec;
                        if (rec->prev) rec->prev->next = rec;
                        if (rec->next) rec->next->prev = rec;
                        dbuf = (char *) rec + sizeof (*rec);
                    }
                }
                memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
                dbuf[ksiz + psiz + vsiz] = '\0';
                rec->vsiz = vsiz;
                if (map->last != rec) {
                    if (map->first == rec) map->first = rec->next;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    rec->prev = map->last;
                    rec->next = NULL;
                    map->last->next = rec;
                    map->last = rec;
                }
                return;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
}

/* Store a record of the value of two regions into a map object. */
void tcmapput4(TCMAP *map, const void *kbuf, int ksiz,
        const void *fvbuf, int fvsiz, const void *lvbuf, int lvsiz) {
    assert(map && kbuf && ksiz >= 0 && fvbuf && fvsiz >= 0 && lvbuf && lvsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                int vsiz = fvsiz + lvsiz;
                map->msiz += vsiz - rec->vsiz;
                int psiz = TCALIGNPAD(ksiz);
                ksiz += psiz;
                if (vsiz > rec->vsiz) {
                    TCMAPREC *old = rec;
                    TCREALLOC(rec, rec, sizeof (*rec) + ksiz + vsiz + 1);
                    if (rec != old) {
                        if (map->first == old) map->first = rec;
                        if (map->last == old) map->last = rec;
                        if (map->cur == old) map->cur = rec;
                        *entp = rec;
                        if (rec->prev) rec->prev->next = rec;
                        if (rec->next) rec->next->prev = rec;
                        dbuf = (char *) rec + sizeof (*rec);
                    }
                }
                memcpy(dbuf + ksiz, fvbuf, fvsiz);
                memcpy(dbuf + ksiz + fvsiz, lvbuf, lvsiz);
                dbuf[ksiz + vsiz] = '\0';
                rec->vsiz = vsiz;
                return;
            }
        }
    }
    int vsiz = fvsiz + lvsiz;
    int psiz = TCALIGNPAD(ksiz);
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    ksiz += psiz;
    memcpy(dbuf + ksiz, fvbuf, fvsiz);
    memcpy(dbuf + ksiz + fvsiz, lvbuf, lvsiz);
    dbuf[ksiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
}

/* Concatenate a value at the existing record and make it semivolatile in a map object. */
void tcmapputcat3(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                map->msiz += vsiz;
                int psiz = TCALIGNPAD(ksiz);
                int asiz = sizeof (*rec) + ksiz + psiz + rec->vsiz + vsiz + 1;
                int unit = (asiz <= TCMAPCSUNIT) ? TCMAPCSUNIT : TCMAPCBUNIT;
                asiz = (asiz - 1) + unit - (asiz - 1) % unit;
                TCMAPREC *old = rec;
                TCREALLOC(rec, rec, asiz);
                if (rec != old) {
                    if (map->first == old) map->first = rec;
                    if (map->last == old) map->last = rec;
                    if (map->cur == old) map->cur = rec;
                    *entp = rec;
                    if (rec->prev) rec->prev->next = rec;
                    if (rec->next) rec->next->prev = rec;
                    dbuf = (char *) rec + sizeof (*rec);
                }
                memcpy(dbuf + ksiz + psiz + rec->vsiz, vbuf, vsiz);
                rec->vsiz += vsiz;
                dbuf[ksiz + psiz + rec->vsiz] = '\0';
                if (map->last != rec) {
                    if (map->first == rec) map->first = rec->next;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    rec->prev = map->last;
                    rec->next = NULL;
                    map->last->next = rec;
                    map->last = rec;
                }
                return;
            }
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    int asiz = sizeof (*rec) + ksiz + psiz + vsiz + 1;
    int unit = (asiz <= TCMAPCSUNIT) ? TCMAPCSUNIT : TCMAPCBUNIT;
    asiz = (asiz - 1) + unit - (asiz - 1) % unit;
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, asiz);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
}

/* Store a record into a map object with a duplication handler. */
bool tcmapputproc(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
        TCPDPROC proc, void *op) {
    assert(map && kbuf && ksiz >= 0 && proc);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    int bidx = hash % map->bnum;
    TCMAPREC *rec = map->buckets[bidx];
    TCMAPREC **entp = map->buckets + bidx;
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (hash < rhash) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                entp = &(rec->left);
                rec = rec->left;
            } else if (kcmp > 0) {
                entp = &(rec->right);
                rec = rec->right;
            } else {
                int psiz = TCALIGNPAD(ksiz);
                int nvsiz;
                char *nvbuf = proc(dbuf + ksiz + psiz, rec->vsiz, &nvsiz, op);
                if (nvbuf == (void *) - 1) {
                    map->rnum--;
                    map->msiz -= rksiz + rec->vsiz;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    if (rec == map->first) map->first = rec->next;
                    if (rec == map->last) map->last = rec->prev;
                    if (rec == map->cur) map->cur = rec->next;
                    if (rec->left && !rec->right) {
                        *entp = rec->left;
                    } else if (!rec->left && rec->right) {
                        *entp = rec->right;
                    } else if (!rec->left && !rec->left) {
                        *entp = NULL;
                    } else {
                        *entp = rec->left;
                        TCMAPREC *tmp = *entp;
                        while (tmp->right) {
                            tmp = tmp->right;
                        }
                        tmp->right = rec->right;
                    }
                    TCFREE(rec);
                    return true;
                }
                if (!nvbuf) return false;
                map->msiz += nvsiz - rec->vsiz;
                if (nvsiz > rec->vsiz) {
                    TCMAPREC *old = rec;
                    TCREALLOC(rec, rec, sizeof (*rec) + ksiz + psiz + nvsiz + 1);
                    if (rec != old) {
                        if (map->first == old) map->first = rec;
                        if (map->last == old) map->last = rec;
                        if (map->cur == old) map->cur = rec;
                        *entp = rec;
                        if (rec->prev) rec->prev->next = rec;
                        if (rec->next) rec->next->prev = rec;
                        dbuf = (char *) rec + sizeof (*rec);
                    }
                }
                memcpy(dbuf + ksiz + psiz, nvbuf, nvsiz);
                dbuf[ksiz + psiz + nvsiz] = '\0';
                rec->vsiz = nvsiz;
                TCFREE(nvbuf);
                return true;
            }
        }
    }
    if (!vbuf) return false;
    int psiz = TCALIGNPAD(ksiz);
    int asiz = sizeof (*rec) + ksiz + psiz + vsiz + 1;
    int unit = (asiz <= TCMAPCSUNIT) ? TCMAPCSUNIT : TCMAPCBUNIT;
    asiz = (asiz - 1) + unit - (asiz - 1) % unit;
    map->msiz += ksiz + vsiz;
    TCMALLOC(rec, asiz);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz | hash;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    rec->prev = map->last;
    rec->next = NULL;
    *entp = rec;
    if (!map->first) map->first = rec;
    if (map->last) map->last->next = rec;
    map->last = rec;
    map->rnum++;
    return true;
}

/* Retrieve a semivolatile record in a map object. */
const void *tcmapget3(TCMAP *map, const void *kbuf, int ksiz, int *sp) {
    assert(map && kbuf && ksiz >= 0 && sp);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    TCMAPREC *rec = map->buckets[hash % map->bnum];
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            rec = rec->left;
        } else if (hash < rhash) {
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                rec = rec->left;
            } else if (kcmp > 0) {
                rec = rec->right;
            } else {
                if (map->last != rec) {
                    if (map->first == rec) map->first = rec->next;
                    if (rec->prev) rec->prev->next = rec->next;
                    if (rec->next) rec->next->prev = rec->prev;
                    rec->prev = map->last;
                    rec->next = NULL;
                    map->last->next = rec;
                    map->last = rec;
                }
                *sp = rec->vsiz;
                return dbuf + rksiz + TCALIGNPAD(rksiz);
            }
        }
    }
    return NULL;
}

/* Retrieve a string record in a map object with specifying the default value string. */
const char *tcmapget4(TCMAP *map, const char *kstr, const char *dstr) {
    assert(map && kstr && dstr);
    int vsiz;
    const char *vbuf = tcmapget(map, kstr, strlen(kstr), &vsiz);
    return vbuf ? vbuf : dstr;
}

/* Initialize the iterator of a map object at the record corresponding a key. */
void tcmapiterinit2(TCMAP *map, const void *kbuf, int ksiz) {
    assert(map && kbuf && ksiz >= 0);
    if (ksiz > TCMAPKMAXSIZ) ksiz = TCMAPKMAXSIZ;
    uint32_t hash;
    TCMAPHASH1(hash, kbuf, ksiz);
    TCMAPREC *rec = map->buckets[hash % map->bnum];
    TCMAPHASH2(hash, kbuf, ksiz);
    hash &= ~TCMAPKMAXSIZ;
    while (rec) {
        uint32_t rhash = rec->ksiz & ~TCMAPKMAXSIZ;
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        if (hash > rhash) {
            rec = rec->left;
        } else if (hash < rhash) {
            rec = rec->right;
        } else {
            char *dbuf = (char *) rec + sizeof (*rec);
            int kcmp = TCKEYCMP(kbuf, ksiz, dbuf, rksiz);
            if (kcmp < 0) {
                rec = rec->left;
            } else if (kcmp > 0) {
                rec = rec->right;
            } else {
                map->cur = rec;
                return;
            }
        }
    }
}

/* Initialize the iterator of a map object at the record corresponding a key string. */
void tcmapiterinit3(TCMAP *map, const char *kstr) {
    assert(map && kstr);
    tcmapiterinit2(map, kstr, strlen(kstr));
}

/* Get the value bound to the key fetched from the iterator of a map object. */
const void *tcmapiterval(const void *kbuf, int *sp) {
    assert(kbuf && sp);
    TCMAPREC *rec = (TCMAPREC *) ((char *) kbuf - sizeof (*rec));
    uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
    *sp = rec->vsiz;
    return (char *) kbuf + rksiz + TCALIGNPAD(rksiz);
}

/* Get the value string bound to the key fetched from the iterator of a map object. */
const char *tcmapiterval2(const char *kstr) {
    assert(kstr);
    TCMAPREC *rec = (TCMAPREC *) (kstr - sizeof (*rec));
    uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
    return kstr + rksiz + TCALIGNPAD(rksiz);
}

/* Create an array of strings of all keys in a map object. */
const char **tcmapkeys2(const TCMAP *map, int *np) {
    assert(map && np);
    const char **ary;
    TCMALLOC(ary, sizeof (*ary) * map->rnum + 1);
    int anum = 0;
    TCMAPREC *rec = map->first;
    while (rec) {
        ary[(anum++)] = (char *) rec + sizeof (*rec);
        rec = rec->next;
    }
    *np = anum;
    return ary;
}

/* Create an array of strings of all values in a map object. */
const char **tcmapvals2(const TCMAP *map, int *np) {
    assert(map && np);
    const char **ary;
    TCMALLOC(ary, sizeof (*ary) * map->rnum + 1);
    int anum = 0;
    TCMAPREC *rec = map->first;
    while (rec) {
        uint32_t rksiz = rec->ksiz & TCMAPKMAXSIZ;
        ary[(anum++)] = (char *) rec + sizeof (*rec) + rksiz + TCALIGNPAD(rksiz);
        rec = rec->next;
    }
    *np = anum;
    return ary;
}

/* Extract a map record from a serialized byte array. */
void *tcmaploadone(const void *ptr, int size, const void *kbuf, int ksiz, int *sp) {
    assert(ptr && size >= 0 && kbuf && ksiz >= 0 && sp);
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        if (rsiz == ksiz && !memcmp(kbuf, rp, rsiz)) {
            rp += rsiz;
            TCREADVNUMBUF(rp, rsiz, step);
            rp += step;
            *sp = rsiz;
            char *rv;
            TCMEMDUP(rv, rp, rsiz);
            return rv;
        }
        rp += rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        rp += rsiz;
    }
    return NULL;
}

/* Extract a map record from a serialized byte array into extensible string object. */
int tcmaploadoneintoxstr(const void *ptr, int size, const void *kbuf, int ksiz, TCXSTR *xstr) {
    assert(ptr && size >= 0 && kbuf && ksiz >= 0 && xstr);
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        if (rsiz == ksiz && !memcmp(kbuf, rp, rsiz)) {
            rp += rsiz;
            TCREADVNUMBUF(rp, rsiz, step);
            rp += step;
            TCXSTRCAT(xstr, rp, rsiz);
            return rsiz;
        }
        rp += rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        rp += rsiz;
    }
    return -1;
}

/* Perform formatted output into a map object. */
void tcmapprintf(TCMAP *map, const char *kstr, const char *format, ...) {
    assert(map && kstr && format);
    TCXSTR *xstr = tcxstrnew();
    va_list ap;
    va_start(ap, format);
    tcvxstrprintf(xstr, format, ap);
    va_end(ap);
    tcmapput(map, kstr, strlen(kstr), TCXSTRPTR(xstr), TCXSTRSIZE(xstr));
    tcxstrdel(xstr);
}



/*************************************************************************************************
 * ordered tree
 *************************************************************************************************/


#define TREESTACKNUM   2048              // capacity of the stack of ordered tree
#define TCTREECSUNIT   52                // small allocation unit size of map concatenation
#define TCTREECBUNIT   252               // big allocation unit size of map concatenation


/* private function prototypes */
static TCTREEREC* tctreesplay(TCTREE *tree, const void *kbuf, int ksiz);

/* Create a tree object. */
TCTREE *tctreenew(void) {
    return tctreenew2(tccmplexical, NULL);
}

/* Create a tree object with specifying the custom comparison function. */
TCTREE *tctreenew2(TCCMP cmp, void *cmpop) {
    assert(cmp);
    TCTREE *tree;
    TCMALLOC(tree, sizeof (*tree));
    tree->root = NULL;
    tree->cur = NULL;
    tree->rnum = 0;
    tree->msiz = 0;
    tree->cmp = cmp;
    tree->cmpop = cmpop;
    return tree;
}

/* Copy a tree object. */
TCTREE *tctreedup(const TCTREE *tree) {
    assert(tree);
    TCTREE *ntree = tctreenew2(tree->cmp, tree->cmpop);
    if (tree->root) {
        TCTREEREC * histbuf[TREESTACKNUM];
        TCTREEREC **history = histbuf;
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (hnum >= TREESTACKNUM - 2 && history == histbuf) {
                TCMALLOC(history, sizeof (*history) * tree->rnum);
                memcpy(history, histbuf, sizeof (*history) * hnum);
            }
            if (rec->left) history[hnum++] = rec->left;
            if (rec->right) history[hnum++] = rec->right;
            char *dbuf = (char *) rec + sizeof (*rec);
            tctreeput(ntree, dbuf, rec->ksiz, dbuf + rec->ksiz + TCALIGNPAD(rec->ksiz), rec->vsiz);
        }
        if (history != histbuf) TCFREE(history);
    }
    return ntree;
}

/* Delete a tree object. */
void tctreedel(TCTREE *tree) {
    assert(tree);
    if (tree->root) {
        TCTREEREC * histbuf[TREESTACKNUM];
        TCTREEREC **history = histbuf;
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (hnum >= TREESTACKNUM - 2 && history == histbuf) {
                TCMALLOC(history, sizeof (*history) * tree->rnum);
                memcpy(history, histbuf, sizeof (*history) * hnum);
            }
            if (rec->left) history[hnum++] = rec->left;
            if (rec->right) history[hnum++] = rec->right;
            TCFREE(rec);
        }
        if (history != histbuf) TCFREE(history);
    }
    TCFREE(tree);
}

/* Store a record into a tree object. */
void tctreeput(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + vsiz;
        return;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else if (cv > 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else {
        tree->msiz += vsiz - top->vsiz;
        int psiz = TCALIGNPAD(ksiz);
        if (vsiz > top->vsiz) {
            TCTREEREC *old = top;
            TCREALLOC(top, top, sizeof (*top) + ksiz + psiz + vsiz + 1);
            if (top != old) {
                if (tree->cur == old) tree->cur = top;
                dbuf = (char *) top + sizeof (*top);
            }
        }
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        top->vsiz = vsiz;
        tree->root = top;
    }
}

/* Store a string record into a tree object. */
void tctreeput2(TCTREE *tree, const char *kstr, const char *vstr) {
    assert(tree && kstr && vstr);
    tctreeput(tree, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a new record into a tree object. */
bool tctreeputkeep(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + vsiz;
        return true;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else if (cv > 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else {
        tree->root = top;
        return false;
    }
    return true;
}

/* Store a new string record into a tree object. */
bool tctreeputkeep2(TCTREE *tree, const char *kstr, const char *vstr) {
    assert(tree && kstr && vstr);
    return tctreeputkeep(tree, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Concatenate a value at the end of the value of the existing record in a tree object. */
void tctreeputcat(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + vsiz;
        return;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else if (cv > 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else {
        tree->msiz += vsiz;
        int psiz = TCALIGNPAD(ksiz);
        int asiz = sizeof (*top) + ksiz + psiz + top->vsiz + vsiz + 1;
        int unit = (asiz <= TCTREECSUNIT) ? TCTREECSUNIT : TCTREECBUNIT;
        asiz = (asiz - 1) + unit - (asiz - 1) % unit;
        TCTREEREC *old = top;
        TCREALLOC(top, top, asiz);
        if (top != old) {
            if (tree->cur == old) tree->cur = top;
            dbuf = (char *) top + sizeof (*top);
        }
        memcpy(dbuf + ksiz + psiz + top->vsiz, vbuf, vsiz);
        top->vsiz += vsiz;
        dbuf[ksiz + psiz + top->vsiz] = '\0';
        tree->root = top;
    }
}

/* Concatenate a string value at the end of the value of the existing record in a tree object. */
void tctreeputcat2(TCTREE *tree, const char *kstr, const char *vstr) {
    assert(tree && kstr && vstr);
    tctreeputcat(tree, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a record into a tree object with a duplication handler. */
bool tctreeputproc(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
        TCPDPROC proc, void *op) {
    assert(tree && kbuf && ksiz >= 0 && proc);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        if (!vbuf) return false;
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + vsiz;
        return true;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        if (!vbuf) {
            tree->root = top;
            return false;
        }
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else if (cv > 0) {
        if (!vbuf) {
            tree->root = top;
            return false;
        }
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
        dbuf[ksiz + psiz + vsiz] = '\0';
        rec->vsiz = vsiz;
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + vsiz;
        tree->root = rec;
    } else {
        int psiz = TCALIGNPAD(ksiz);
        int nvsiz;
        char *nvbuf = proc(dbuf + ksiz + psiz, top->vsiz, &nvsiz, op);
        if (nvbuf == (void *) - 1) {
            tree->rnum--;
            tree->msiz -= top->ksiz + top->vsiz;
            if (tree->cur == top) {
                TCTREEREC *rec = top->right;
                if (rec) {
                    while (rec->left) {
                        rec = rec->left;
                    }
                }
                tree->cur = rec;
            }
            if (!top->left) {
                tree->root = top->right;
            } else if (!top->right) {
                tree->root = top->left;
            } else {
                tree->root = top->left;
                TCTREEREC *rec = tctreesplay(tree, kbuf, ksiz);
                rec->right = top->right;
                tree->root = rec;
            }
            TCFREE(top);
            return true;
        }
        if (!nvbuf) {
            tree->root = top;
            return false;
        }
        tree->msiz += nvsiz - top->vsiz;
        if (nvsiz > top->vsiz) {
            TCTREEREC *old = top;
            TCREALLOC(top, top, sizeof (*top) + ksiz + psiz + nvsiz + 1);
            if (top != old) {
                if (tree->cur == old) tree->cur = top;
                dbuf = (char *) top + sizeof (*top);
            }
        }
        memcpy(dbuf + ksiz + psiz, nvbuf, nvsiz);
        dbuf[ksiz + psiz + nvsiz] = '\0';
        top->vsiz = nvsiz;
        TCFREE(nvbuf);
        tree->root = top;
    }
    return true;
}

/* Remove a record of a tree object. */
bool tctreeout(TCTREE *tree, const void *kbuf, int ksiz) {
    assert(tree && kbuf && ksiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) return false;
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv != 0) {
        tree->root = top;
        return false;
    }
    tree->rnum--;
    tree->msiz -= top->ksiz + top->vsiz;
    if (tree->cur == top) {
        TCTREEREC *rec = top->right;
        if (rec) {
            while (rec->left) {
                rec = rec->left;
            }
        }
        tree->cur = rec;
    }
    if (!top->left) {
        tree->root = top->right;
    } else if (!top->right) {
        tree->root = top->left;
    } else {
        tree->root = top->left;
        TCTREEREC *rec = tctreesplay(tree, kbuf, ksiz);
        rec->right = top->right;
        tree->root = rec;
    }
    TCFREE(top);
    return true;
}

/* Remove a string record of a tree object. */
bool tctreeout2(TCTREE *tree, const char *kstr) {
    assert(tree && kstr);
    return tctreeout(tree, kstr, strlen(kstr));
}

/* Retrieve a record in a tree object. */
const void *tctreeget(TCTREE *tree, const void *kbuf, int ksiz, int *sp) {
    assert(tree && kbuf && ksiz >= 0 && sp);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) return NULL;
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv != 0) {
        tree->root = top;
        return NULL;
    }
    tree->root = top;
    *sp = top->vsiz;
    return dbuf + top->ksiz + TCALIGNPAD(top->ksiz);
}

/* Retrieve a string record in a tree object. */
const char *tctreeget2(TCTREE *tree, const char *kstr) {
    assert(tree && kstr);
    int vsiz;
    return tctreeget(tree, kstr, strlen(kstr), &vsiz);
}

/* Initialize the iterator of a tree object. */
void tctreeiterinit(TCTREE *tree) {
    assert(tree);
    TCTREEREC *rec = tree->root;
    if (!rec) return;
    while (rec->left) {
        rec = rec->left;
    }
    tree->cur = rec;
}

/* Get the next key of the iterator of a tree object. */
const void *tctreeiternext(TCTREE *tree, int *sp) {
    assert(tree && sp);
    if (!tree->cur) return NULL;
    TCTREEREC *rec = tree->cur;
    const char *kbuf = (char *) rec + sizeof (*rec);
    int ksiz = rec->ksiz;
    rec = tctreesplay(tree, kbuf, ksiz);
    if (!rec) return NULL;
    tree->root = rec;
    rec = rec->right;
    if (rec) {
        while (rec->left) {
            rec = rec->left;
        }
    }
    tree->cur = rec;
    *sp = ksiz;
    return kbuf;
}

/* Get the next key string of the iterator of a tree object. */
const char *tctreeiternext2(TCTREE *tree) {
    assert(tree);
    int ksiz;
    return tctreeiternext(tree, &ksiz);
}

/* Get the number of records stored in a tree object. */
uint64_t tctreernum(const TCTREE *tree) {
    assert(tree);
    return tree->rnum;
}

/* Get the total size of memory used in a tree object. */
uint64_t tctreemsiz(const TCTREE *tree) {
    assert(tree);
    return tree->msiz + tree->rnum * (sizeof (*tree->root) + sizeof (tcgeneric_t));
}

/* Create a list object containing all keys in a tree object. */
TCLIST *tctreekeys(const TCTREE *tree) {
    assert(tree);
    TCLIST *list = tclistnew2(tree->rnum);
    if (tree->root) {
        TCTREEREC **history;
        TCMALLOC(history, sizeof (*history) * tree->rnum);
        TCTREEREC **result;
        TCMALLOC(result, sizeof (*history) * tree->rnum);
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (!rec) {
                rec = result[hnum];
                char *dbuf = (char *) rec + sizeof (*rec);
                TCLISTPUSH(list, dbuf, rec->ksiz);
                continue;
            }
            if (rec->right) history[hnum++] = rec->right;
            history[hnum] = NULL;
            result[hnum] = rec;
            hnum++;
            if (rec->left) history[hnum++] = rec->left;
        }
        TCFREE(result);
        TCFREE(history);
    }
    return list;
}

/* Create a list object containing all values in a tree object. */
TCLIST *tctreevals(const TCTREE *tree) {
    assert(tree);
    TCLIST *list = tclistnew2(tree->rnum);
    if (tree->root) {
        TCTREEREC **history;
        TCMALLOC(history, sizeof (*history) * tree->rnum);
        TCTREEREC **result;
        TCMALLOC(result, sizeof (*history) * tree->rnum);
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (!rec) {
                rec = result[hnum];
                char *dbuf = (char *) rec + sizeof (*rec);
                TCLISTPUSH(list, dbuf + rec->ksiz + TCALIGNPAD(rec->ksiz), rec->vsiz);
                continue;
            }
            if (rec->right) history[hnum++] = rec->right;
            history[hnum] = NULL;
            result[hnum] = rec;
            hnum++;
            if (rec->left) history[hnum++] = rec->left;
        }
        TCFREE(result);
        TCFREE(history);
    }
    return list;
}

/* Add an integer to a record in a tree object. */
int tctreeaddint(TCTREE *tree, const void *kbuf, int ksiz, int num) {
    assert(tree && kbuf && ksiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + sizeof (num);
        return num;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + sizeof (num);
        tree->root = rec;
    } else if (cv > 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + sizeof (num);
        tree->root = rec;
    } else {
        tree->root = top;
        if (top->vsiz != sizeof (num)) return INT_MIN;
        int *resp = (int *) (dbuf + ksiz + TCALIGNPAD(ksiz));
        return *resp += num;
    }
    return num;
}

/* Add a real number to a record in a tree object. */
double tctreeadddouble(TCTREE *tree, const void *kbuf, int ksiz, double num) {
    assert(tree && kbuf && ksiz >= 0);
    TCTREEREC *top = tctreesplay(tree, kbuf, ksiz);
    if (!top) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        char *dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = NULL;
        rec->right = NULL;
        tree->root = rec;
        tree->rnum = 1;
        tree->msiz = ksiz + sizeof (num);
        return num;
    }
    char *dbuf = (char *) top + sizeof (*top);
    int cv = tree->cmp(kbuf, ksiz, dbuf, top->ksiz, tree->cmpop);
    if (cv < 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = top->left;
        rec->right = top;
        top->left = NULL;
        tree->rnum++;
        tree->msiz += ksiz + sizeof (num);
        tree->root = rec;
    } else if (cv > 0) {
        int psiz = TCALIGNPAD(ksiz);
        TCTREEREC *rec;
        TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + sizeof (num) + 1);
        dbuf = (char *) rec + sizeof (*rec);
        memcpy(dbuf, kbuf, ksiz);
        dbuf[ksiz] = '\0';
        rec->ksiz = ksiz;
        memcpy(dbuf + ksiz + psiz, &num, sizeof (num));
        dbuf[ksiz + psiz + sizeof (num)] = '\0';
        rec->vsiz = sizeof (num);
        rec->left = top;
        rec->right = top->right;
        top->right = NULL;
        tree->rnum++;
        tree->msiz += ksiz + sizeof (num);
        tree->root = rec;
    } else {
        tree->root = top;
        if (top->vsiz != sizeof (num)) return nan("");
        double *resp = (double *) (dbuf + ksiz + TCALIGNPAD(ksiz));
        return *resp += num;
    }
    return num;
}

/* Clear a tree object. */
void tctreeclear(TCTREE *tree) {
    assert(tree);
    if (tree->root) {
        TCTREEREC * histbuf[TREESTACKNUM];
        TCTREEREC **history = histbuf;
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (hnum >= TREESTACKNUM - 2 && history == histbuf) {
                TCMALLOC(history, sizeof (*history) * tree->rnum);
                memcpy(history, histbuf, sizeof (*history) * hnum);
            }
            if (rec->left) history[hnum++] = rec->left;
            if (rec->right) history[hnum++] = rec->right;
            TCFREE(rec);
        }
        if (history != histbuf) TCFREE(history);
    }
    tree->root = NULL;
    tree->cur = NULL;
    tree->rnum = 0;
    tree->msiz = 0;
}

/* Remove fringe records of a tree object. */
void tctreecutfringe(TCTREE *tree, int num) {
    assert(tree && num >= 0);
    if (!tree->root || num < 1) return;
    TCTREEREC **history;
    TCMALLOC(history, sizeof (*history) * tree->rnum);
    int hnum = 0;
    history[hnum++] = tree->root;
    for (int i = 0; i < hnum; i++) {
        TCTREEREC *rec = history[i];
        if (rec->left) history[hnum++] = rec->left;
        if (rec->right) history[hnum++] = rec->right;
    }
    TCTREEREC *cur = NULL;
    for (int i = hnum - 1; i >= 0; i--) {
        TCTREEREC *rec = history[i];
        if (rec->left) {
            TCTREEREC *child = rec->left;
            tree->rnum--;
            tree->msiz -= child->ksiz + child->vsiz;
            rec->left = NULL;
            if (tree->cur == child) {
                tree->cur = NULL;
                cur = child;
            } else {
                TCFREE(child);
            }
            if (--num < 1) break;
        }
        if (rec->right) {
            TCTREEREC *child = rec->right;
            tree->rnum--;
            tree->msiz -= child->ksiz + child->vsiz;
            rec->right = NULL;
            if (tree->cur == child) {
                tree->cur = NULL;
                cur = child;
            } else {
                TCFREE(child);
            }
            if (--num < 1) break;
        }
    }
    if (num > 0) {
        TCFREE(tree->root);
        tree->root = NULL;
        tree->cur = NULL;
        tree->rnum = 0;
        tree->msiz = 0;
    }
    if (cur) {
        char *dbuf = (char *) cur + sizeof (*cur);
        tctreeiterinit2(tree, dbuf, cur->ksiz);
        TCFREE(cur);
    }
    TCFREE(history);
}

/* Serialize a tree object into a byte array. */
void *tctreedump(const TCTREE *tree, int *sp) {
    assert(tree && sp);
    int tsiz = 0;
    if (tree->root) {
        TCTREEREC * histbuf[TREESTACKNUM];
        TCTREEREC **history = histbuf;
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (hnum >= TREESTACKNUM - 2 && history == histbuf) {
                TCMALLOC(history, sizeof (*history) * tree->rnum);
                memcpy(history, histbuf, sizeof (*history) * hnum);
            }
            if (rec->left) history[hnum++] = rec->left;
            if (rec->right) history[hnum++] = rec->right;
            tsiz += rec->ksiz + rec->vsiz + sizeof (int) * 2;
        }
        if (history != histbuf) TCFREE(history);
    }
    char *buf;
    TCMALLOC(buf, tsiz + 1);
    char *wp = buf;
    if (tree->root) {
        TCTREEREC * histbuf[TREESTACKNUM];
        TCTREEREC **history = histbuf;
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (hnum >= TREESTACKNUM - 2 && history == histbuf) {
                TCMALLOC(history, sizeof (*history) * tree->rnum);
                memcpy(history, histbuf, sizeof (*history) * hnum);
            }
            if (rec->left) history[hnum++] = rec->left;
            if (rec->right) history[hnum++] = rec->right;
            const char *kbuf = (char *) rec + sizeof (*rec);
            int ksiz = rec->ksiz;
            const char *vbuf = kbuf + ksiz + TCALIGNPAD(ksiz);
            int vsiz = rec->vsiz;
            int step;
            TCSETVNUMBUF(step, wp, ksiz);
            wp += step;
            memcpy(wp, kbuf, ksiz);
            wp += ksiz;
            TCSETVNUMBUF(step, wp, vsiz);
            wp += step;
            memcpy(wp, vbuf, vsiz);
            wp += vsiz;
        }
        if (history != histbuf) TCFREE(history);
    }
    *sp = wp - buf;
    return buf;
}

/* Create a tree object from a serialized byte array. */
TCTREE *tctreeload(const void *ptr, int size, TCCMP cmp, void *cmpop) {
    assert(ptr && size >= 0 && cmp);
    TCTREE *tree = tctreenew2(cmp, cmpop);
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, ksiz, vsiz;
        TCREADVNUMBUF(rp, ksiz, step);
        rp += step;
        const char *kbuf = rp;
        rp += ksiz;
        TCREADVNUMBUF(rp, vsiz, step);
        rp += step;
        tctreeputkeep(tree, kbuf, ksiz, rp, vsiz);
        rp += vsiz;
    }
    return tree;
}

/* Perform the splay operation of a tree object.
   `tree' specifies the tree object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   The return value is the pointer to the record corresponding the key. */
static TCTREEREC* tctreesplay(TCTREE *tree, const void *kbuf, int ksiz) {
    assert(tree && kbuf && ksiz >= 0);
    TCTREEREC *top = tree->root;
    if (!top) return NULL;
    TCCMP cmp = tree->cmp;
    void *cmpop = tree->cmpop;
    TCTREEREC ent;
    ent.left = NULL;
    ent.right = NULL;
    TCTREEREC *lrec = &ent;
    TCTREEREC *rrec = &ent;
    while (true) {
        char *dbuf = (char *) top + sizeof (*top);
        int cv = cmp(kbuf, ksiz, dbuf, top->ksiz, cmpop);
        if (cv < 0) {
            if (!top->left) break;
            dbuf = (char *) top->left + sizeof (*top);
            cv = cmp(kbuf, ksiz, dbuf, top->left->ksiz, cmpop);
            if (cv < 0) {
                TCTREEREC *swap = top->left;
                top->left = swap->right;
                swap->right = top;
                top = swap;
                if (!top->left) break;
            }
            rrec->left = top;
            rrec = top;
            top = top->left;
        } else if (cv > 0) {
            if (!top->right) break;
            dbuf = (char *) top->right + sizeof (*top);
            cv = cmp(kbuf, ksiz, dbuf, top->right->ksiz, cmpop);
            if (cv > 0) {
                TCTREEREC *swap = top->right;
                top->right = swap->left;
                swap->left = top;
                top = swap;
                if (!top->right) break;
            }
            lrec->right = top;
            lrec = top;
            top = top->right;
        } else {
            break;
        }
    }
    lrec->right = top->left;
    rrec->left = top->right;
    top->left = ent.right;
    top->right = ent.left;
    return top;
}



/*************************************************************************************************
 * ordered tree (for experts)
 *************************************************************************************************/

/* Store a record into a tree object without balancing nodes. */
void tctreeput3(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *rec = tree->root;
    TCTREEREC **entp = NULL;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        int cv = tree->cmp(kbuf, ksiz, dbuf, rec->ksiz, tree->cmpop);
        if (cv < 0) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (cv > 0) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            tree->msiz += vsiz - rec->vsiz;
            int psiz = TCALIGNPAD(ksiz);
            if (vsiz > rec->vsiz) {
                TCTREEREC *old = rec;
                TCREALLOC(rec, rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
                if (rec != old) {
                    if (tree->root == old) tree->root = rec;
                    if (tree->cur == old) tree->cur = rec;
                    if (entp) *entp = rec;
                    dbuf = (char *) rec + sizeof (*rec);
                }
            }
            memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
            dbuf[ksiz + psiz + vsiz] = '\0';
            rec->vsiz = vsiz;
            return;
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    if (entp) {
        *entp = rec;
    } else {
        tree->root = rec;
    }
    tree->rnum++;
    tree->msiz += ksiz + vsiz;
}

/* Store a new record into a map object without balancing nodes. */
bool tctreeputkeep3(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *rec = tree->root;
    TCTREEREC **entp = NULL;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        int cv = tree->cmp(kbuf, ksiz, dbuf, rec->ksiz, tree->cmpop);
        if (cv < 0) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (cv > 0) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            return false;
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    if (entp) {
        *entp = rec;
    } else {
        tree->root = rec;
    }
    tree->rnum++;
    tree->msiz += ksiz + vsiz;
    return true;
}

/* Concatenate a value at the existing record in a tree object without balancing nodes. */
void tctreeputcat3(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(tree && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCTREEREC *rec = tree->root;
    TCTREEREC **entp = NULL;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        int cv = tree->cmp(kbuf, ksiz, dbuf, rec->ksiz, tree->cmpop);
        if (cv < 0) {
            entp = &(rec->left);
            rec = rec->left;
        } else if (cv > 0) {
            entp = &(rec->right);
            rec = rec->right;
        } else {
            tree->msiz += vsiz;
            int psiz = TCALIGNPAD(ksiz);
            int asiz = sizeof (*rec) + ksiz + psiz + rec->vsiz + vsiz + 1;
            int unit = (asiz <= TCTREECSUNIT) ? TCTREECSUNIT : TCTREECBUNIT;
            asiz = (asiz - 1) + unit - (asiz - 1) % unit;
            TCTREEREC *old = rec;
            TCREALLOC(rec, rec, asiz);
            if (rec != old) {
                if (tree->root == old) tree->root = rec;
                if (tree->cur == old) tree->cur = rec;
                if (entp) *entp = rec;
                dbuf = (char *) rec + sizeof (*rec);
            }
            memcpy(dbuf + ksiz + psiz + rec->vsiz, vbuf, vsiz);
            rec->vsiz += vsiz;
            dbuf[ksiz + psiz + rec->vsiz] = '\0';
            return;
        }
    }
    int psiz = TCALIGNPAD(ksiz);
    TCMALLOC(rec, sizeof (*rec) + ksiz + psiz + vsiz + 1);
    char *dbuf = (char *) rec + sizeof (*rec);
    memcpy(dbuf, kbuf, ksiz);
    dbuf[ksiz] = '\0';
    rec->ksiz = ksiz;
    memcpy(dbuf + ksiz + psiz, vbuf, vsiz);
    dbuf[ksiz + psiz + vsiz] = '\0';
    rec->vsiz = vsiz;
    rec->left = NULL;
    rec->right = NULL;
    if (entp) {
        *entp = rec;
    } else {
        tree->root = rec;
    }
    tree->rnum++;
    tree->msiz += ksiz + vsiz;
}

/* Retrieve a record in a tree object without balancing nodes. */
const void *tctreeget3(const TCTREE *tree, const void *kbuf, int ksiz, int *sp) {
    assert(tree && kbuf && ksiz >= 0 && sp);
    TCTREEREC *rec = tree->root;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        int cv = tree->cmp(kbuf, ksiz, dbuf, rec->ksiz, tree->cmpop);
        if (cv < 0) {
            rec = rec->left;
        } else if (cv > 0) {
            rec = rec->right;
        } else {
            *sp = rec->vsiz;
            return dbuf + rec->ksiz + TCALIGNPAD(rec->ksiz);
        }
    }
    return NULL;
}

/* Retrieve a string record in a tree object with specifying the default value string. */
const char *tctreeget4(TCTREE *tree, const char *kstr, const char *dstr) {
    assert(tree && kstr && dstr);
    int vsiz;
    const char *vbuf = tctreeget(tree, kstr, strlen(kstr), &vsiz);
    return vbuf ? vbuf : dstr;
}

/* Initialize the iterator of a tree object in front of records corresponding a key. */
void tctreeiterinit2(TCTREE *tree, const void *kbuf, int ksiz) {
    assert(tree && kbuf && ksiz >= 0);
    TCTREEREC *rec = tree->root;
    while (rec) {
        char *dbuf = (char *) rec + sizeof (*rec);
        int cv = tree->cmp(kbuf, ksiz, dbuf, rec->ksiz, tree->cmpop);
        if (cv < 0) {
            tree->cur = rec;
            rec = rec->left;
        } else if (cv > 0) {
            rec = rec->right;
        } else {
            tree->cur = rec;
            return;
        }
    }
}

/* Initialize the iterator of a tree object in front of records corresponding a key string. */
void tctreeiterinit3(TCTREE *tree, const char *kstr) {
    assert(tree);
    tctreeiterinit2(tree, kstr, strlen(kstr));
}

/* Get the value bound to the key fetched from the iterator of a tree object. */
const void *tctreeiterval(const void *kbuf, int *sp) {
    assert(kbuf && sp);
    TCTREEREC *rec = (TCTREEREC *) ((char *) kbuf - sizeof (*rec));
    *sp = rec->vsiz;
    return (char *) kbuf + rec->ksiz + TCALIGNPAD(rec->ksiz);
}

/* Get the value string bound to the key fetched from the iterator of a tree object. */
const char *tctreeiterval2(const char *kstr) {
    assert(kstr);
    TCTREEREC *rec = (TCTREEREC *) (kstr - sizeof (*rec));
    return kstr + rec->ksiz + TCALIGNPAD(rec->ksiz);
}

/* Create an array of strings of all keys in a tree object. */
const char **tctreekeys2(const TCTREE *tree, int *np) {
    assert(tree && np);
    const char **ary;
    TCMALLOC(ary, sizeof (*ary) * tree->rnum + 1);
    int anum = 0;
    if (tree->root) {
        TCTREEREC **history;
        TCMALLOC(history, sizeof (*history) * tree->rnum);
        TCTREEREC **result;
        TCMALLOC(result, sizeof (*history) * tree->rnum);
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (!rec) {
                rec = result[hnum];
                ary[(anum++)] = (char *) rec + sizeof (*rec);
                continue;
            }
            if (rec->right) history[hnum++] = rec->right;
            history[hnum] = NULL;
            result[hnum] = rec;
            hnum++;
            if (rec->left) history[hnum++] = rec->left;
        }
        TCFREE(result);
        TCFREE(history);
    }
    *np = anum;
    return ary;
}

/* Create an array of strings of all values in a tree object. */
const char **tctreevals2(const TCTREE *tree, int *np) {
    assert(tree && np);
    const char **ary;
    TCMALLOC(ary, sizeof (*ary) * tree->rnum + 1);
    int anum = 0;
    if (tree->root) {
        TCTREEREC **history;
        TCMALLOC(history, sizeof (*history) * tree->rnum);
        TCTREEREC **result;
        TCMALLOC(result, sizeof (*history) * tree->rnum);
        int hnum = 0;
        history[hnum++] = tree->root;
        while (hnum > 0) {
            TCTREEREC *rec = history[--hnum];
            if (!rec) {
                rec = result[hnum];
                ary[(anum++)] = (char *) rec + sizeof (*rec);
                continue;
            }
            if (rec->right) history[hnum++] = rec->right;
            history[hnum] = NULL;
            result[hnum] = rec;
            hnum++;
            if (rec->left) history[hnum++] = rec->left;
        }
        TCFREE(result);
        TCFREE(history);
    }
    *np = anum;
    return ary;
}

/* Extract a tree record from a serialized byte array. */
void *tctreeloadone(const void *ptr, int size, const void *kbuf, int ksiz, int *sp) {
    assert(ptr && size >= 0 && kbuf && ksiz >= 0 && sp);
    const char *rp = ptr;
    const char *ep = (char *) ptr + size;
    while (rp < ep) {
        int step, rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        if (rsiz == ksiz && !memcmp(kbuf, rp, rsiz)) {
            rp += rsiz;
            TCREADVNUMBUF(rp, rsiz, step);
            rp += step;
            *sp = rsiz;
            char *rv;
            TCMEMDUP(rv, rp, rsiz);
            return rv;
        }
        rp += rsiz;
        TCREADVNUMBUF(rp, rsiz, step);
        rp += step;
        rp += rsiz;
    }
    return NULL;
}

/* Perform formatted output into a tree object. */
void tctreeprintf(TCTREE *tree, const char *kstr, const char *format, ...) {
    assert(tree && kstr && format);
    TCXSTR *xstr = tcxstrnew();
    va_list ap;
    va_start(ap, format);
    tcvxstrprintf(xstr, format, ap);
    va_end(ap);
    tctreeput(tree, kstr, strlen(kstr), TCXSTRPTR(xstr), TCXSTRSIZE(xstr));
    tcxstrdel(xstr);
}



/*************************************************************************************************
 * on-memory hash database
 *************************************************************************************************/


#define TCMDBMNUM      8                 // number of internal maps
#define TCMDBDEFBNUM   65536             // default number of buckets

/* get the first hash value */
#define TCMDBHASH(TC_res, TC_kbuf, TC_ksiz)                             \
  do {                                                                  \
    (TC_res) = hashmurmur32((TC_kbuf), (TC_ksiz), 0x20130517);          \
    (TC_res) &= TCMDBMNUM - 1;                                          \
  } while(false)

/* Create an on-memory hash database object. */
TCMDB *tcmdbnew(void) {
    return tcmdbnew2(TCMDBDEFBNUM);
}

/* Create an on-memory hash database with specifying the number of the buckets. */
TCMDB *tcmdbnew2(uint32_t bnum) {
    TCMDB *mdb;
    if (bnum < 1) bnum = TCMDBDEFBNUM;
    bnum = bnum / TCMDBMNUM + 17;
    TCMALLOC(mdb, sizeof (*mdb));
    TCMALLOC(mdb->mmtxs, sizeof (pthread_rwlock_t) * TCMDBMNUM);
    TCMALLOC(mdb->imtx, sizeof (pthread_mutex_t));
    TCMALLOC(mdb->maps, sizeof (TCMAP *) * TCMDBMNUM);
    if (pthread_mutex_init(mdb->imtx, NULL) != 0) tcmyfatal("mutex error");
    for (int i = 0; i < TCMDBMNUM; i++) {
        if (pthread_rwlock_init((pthread_rwlock_t *) mdb->mmtxs + i, NULL) != 0)
            tcmyfatal("rwlock error");
        mdb->maps[i] = tcmapnew2(bnum);
    }
    mdb->iter = -1;
    return mdb;
}

/* Delete an on-memory hash database object. */
void tcmdbdel(TCMDB *mdb) {
    assert(mdb);
    for (int i = TCMDBMNUM - 1; i >= 0; i--) {
        tcmapdel(mdb->maps[i]);
        pthread_rwlock_destroy((pthread_rwlock_t *) mdb->mmtxs + i);
    }
    pthread_mutex_destroy(mdb->imtx);
    TCFREE(mdb->maps);
    TCFREE(mdb->imtx);
    TCFREE(mdb->mmtxs);
    TCFREE(mdb);
}

/* Store a record into an on-memory hash database. */
void tcmdbput(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return;
    tcmapput(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
}

/* Store a string record into an on-memory hash database. */
void tcmdbput2(TCMDB *mdb, const char *kstr, const char *vstr) {
    assert(mdb && kstr && vstr);
    tcmdbput(mdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a new record into an on-memory hash database. */
bool tcmdbputkeep(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return false;
    bool rv = tcmapputkeep(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Store a new string record into an on-memory hash database. */
bool tcmdbputkeep2(TCMDB *mdb, const char *kstr, const char *vstr) {
    assert(mdb && kstr && vstr);
    return tcmdbputkeep(mdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Concatenate a value at the end of the existing record in an on-memory hash database. */
void tcmdbputcat(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return;
    tcmapputcat(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
}

/* Concatenate a string at the end of the existing record in an on-memory hash database. */
void tcmdbputcat2(TCMDB *mdb, const char *kstr, const char *vstr) {
    assert(mdb && kstr && vstr);
    tcmdbputcat(mdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Remove a record of an on-memory hash database. */
bool tcmdbout(TCMDB *mdb, const void *kbuf, int ksiz) {
    assert(mdb && kbuf && ksiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return false;
    bool rv = tcmapout(mdb->maps[mi], kbuf, ksiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Remove a string record of an on-memory hash database. */
bool tcmdbout2(TCMDB *mdb, const char *kstr) {
    assert(mdb && kstr);
    return tcmdbout(mdb, kstr, strlen(kstr));
}

/* Retrieve a record in an on-memory hash database. */
void *tcmdbget(TCMDB *mdb, const void *kbuf, int ksiz, int *sp) {
    assert(mdb && kbuf && ksiz >= 0 && sp);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_rdlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return NULL;
    int vsiz;
    const char *vbuf = tcmapget(mdb->maps[mi], kbuf, ksiz, &vsiz);
    char *rv;
    if (vbuf) {
        TCMEMDUP(rv, vbuf, vsiz);
        *sp = vsiz;
    } else {
        rv = NULL;
    }
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Retrieve a string record in an on-memory hash database. */
char *tcmdbget2(TCMDB *mdb, const char *kstr) {
    assert(mdb && kstr);
    int vsiz;
    return tcmdbget(mdb, kstr, strlen(kstr), &vsiz);
}

/* Get the size of the value of a record in an on-memory hash database object. */
int tcmdbvsiz(TCMDB *mdb, const void *kbuf, int ksiz) {
    assert(mdb && kbuf && ksiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_rdlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return -1;
    int vsiz;
    const char *vbuf = tcmapget(mdb->maps[mi], kbuf, ksiz, &vsiz);
    if (!vbuf) vsiz = -1;
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return vsiz;
}

/* Get the size of the value of a string record in an on-memory hash database object. */
int tcmdbvsiz2(TCMDB *mdb, const char *kstr) {
    assert(mdb && kstr);
    return tcmdbvsiz(mdb, kstr, strlen(kstr));
}

/* Initialize the iterator of an on-memory hash database. */
void tcmdbiterinit(TCMDB *mdb) {
    assert(mdb);
    if (pthread_mutex_lock(mdb->imtx) != 0) return;
    for (int i = 0; i < TCMDBMNUM; i++) {
        tcmapiterinit(mdb->maps[i]);
    }
    mdb->iter = 0;
    pthread_mutex_unlock(mdb->imtx);
}

/* Get the next key of the iterator of an on-memory hash database. */
void *tcmdbiternext(TCMDB *mdb, int *sp) {
    assert(mdb && sp);
    if (pthread_mutex_lock(mdb->imtx) != 0) return NULL;
    if (mdb->iter < 0 || mdb->iter >= TCMDBMNUM) {
        pthread_mutex_unlock(mdb->imtx);
        return NULL;
    }
    int mi = mdb->iter;
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) {
        pthread_mutex_unlock(mdb->imtx);
        return NULL;
    }
    int ksiz;
    const char *kbuf;
    while (!(kbuf = tcmapiternext(mdb->maps[mi], &ksiz)) && mi < TCMDBMNUM - 1) {
        pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
        mi = ++mdb->iter;
        if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) {
            pthread_mutex_unlock(mdb->imtx);
            return NULL;
        }
    }
    char *rv;
    if (kbuf) {
        TCMEMDUP(rv, kbuf, ksiz);
        *sp = ksiz;
    } else {
        rv = NULL;
    }
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    pthread_mutex_unlock(mdb->imtx);
    return rv;
}

/* Get the next key string of the iterator of an on-memory hash database. */
char *tcmdbiternext2(TCMDB *mdb) {
    assert(mdb);
    int ksiz;
    return tcmdbiternext(mdb, &ksiz);
}

/* Get forward matching keys in an on-memory hash database object. */
TCLIST *tcmdbfwmkeys(TCMDB *mdb, const void *pbuf, int psiz, int max) {
    assert(mdb && pbuf && psiz >= 0);
    TCLIST* keys = tclistnew();
    if (pthread_mutex_lock(mdb->imtx) != 0) return keys;
    if (max < 0) max = INT_MAX;
    for (int i = 0; i < TCMDBMNUM && TCLISTNUM(keys) < max; i++) {
        if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + i) == 0) {
            TCMAP *map = mdb->maps[i];
            TCMAPREC *cur = map->cur;
            tcmapiterinit(map);
            const char *kbuf;
            int ksiz;
            while (TCLISTNUM(keys) < max && (kbuf = tcmapiternext(map, &ksiz)) != NULL) {
                if (ksiz >= psiz && !memcmp(kbuf, pbuf, psiz)) TCLISTPUSH(keys, kbuf, ksiz);
            }
            map->cur = cur;
            pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + i);
        }
    }
    pthread_mutex_unlock(mdb->imtx);
    return keys;
}

/* Get forward matching string keys in an on-memory hash database object. */
TCLIST *tcmdbfwmkeys2(TCMDB *mdb, const char *pstr, int max) {
    assert(mdb && pstr);
    return tcmdbfwmkeys(mdb, pstr, strlen(pstr), max);
}

/* Get the number of records stored in an on-memory hash database. */
uint64_t tcmdbrnum(TCMDB *mdb) {
    assert(mdb);
    uint64_t rnum = 0;
    for (int i = 0; i < TCMDBMNUM; i++) {
        rnum += tcmaprnum(mdb->maps[i]);
    }
    return rnum;
}

/* Get the total size of memory used in an on-memory hash database object. */
uint64_t tcmdbmsiz(TCMDB *mdb) {
    assert(mdb);
    uint64_t msiz = 0;
    for (int i = 0; i < TCMDBMNUM; i++) {
        msiz += tcmapmsiz(mdb->maps[i]);
    }
    return msiz;
}

/* Add an integer to a record in an on-memory hash database object. */
int tcmdbaddint(TCMDB *mdb, const void *kbuf, int ksiz, int num) {
    assert(mdb && kbuf && ksiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return INT_MIN;
    int rv = tcmapaddint(mdb->maps[mi], kbuf, ksiz, num);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Add a real number to a record in an on-memory hash database object. */
double tcmdbadddouble(TCMDB *mdb, const void *kbuf, int ksiz, double num) {
    assert(mdb && kbuf && ksiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return nan("");
    double rv = tcmapadddouble(mdb->maps[mi], kbuf, ksiz, num);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Clear an on-memory hash database object. */
void tcmdbvanish(TCMDB *mdb) {
    assert(mdb);
    for (int i = 0; i < TCMDBMNUM; i++) {
        if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + i) == 0) {
            tcmapclear(mdb->maps[i]);
            pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + i);
        }
    }
}

/* Remove front records of a map object. */
void tcmdbcutfront(TCMDB *mdb, int num) {
    assert(mdb && num >= 0);
    num = num / TCMDBMNUM + 1;
    for (int i = 0; i < TCMDBMNUM; i++) {
        if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + i) == 0) {
            tcmapcutfront(mdb->maps[i], num);
            pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + i);
        }
    }
}



/*************************************************************************************************
 * on-memory hash database (for experts)
 *************************************************************************************************/

/* Store a record and make it semivolatile in an on-memory hash database object. */
void tcmdbput3(TCMDB *mdb, const void *kbuf, int ksiz, const char *vbuf, int vsiz) {
    assert(mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return;
    tcmapput3(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
}

/* Store a record of the value of two regions into an on-memory hash database object. */
void tcmdbput4(TCMDB *mdb, const void *kbuf, int ksiz,
        const void *fvbuf, int fvsiz, const void *lvbuf, int lvsiz) {
    assert(mdb && kbuf && ksiz >= 0 && fvbuf && fvsiz >= 0 && lvbuf && lvsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return;
    tcmapput4(mdb->maps[mi], kbuf, ksiz, fvbuf, fvsiz, lvbuf, lvsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
}

/* Concatenate a value and make it semivolatile in on-memory hash database object. */
void tcmdbputcat3(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return;
    tcmapputcat3(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
}

/* Store a record into a on-memory hash database object with a duplication handler. */
bool tcmdbputproc(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
        TCPDPROC proc, void *op) {
    assert(mdb && kbuf && ksiz >= 0 && proc);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return false;
    bool rv = tcmapputproc(mdb->maps[mi], kbuf, ksiz, vbuf, vsiz, proc, op);
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Retrieve a record and move it astern in an on-memory hash database. */
void *tcmdbget3(TCMDB *mdb, const void *kbuf, int ksiz, int *sp) {
    assert(mdb && kbuf && ksiz >= 0 && sp);
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) return NULL;
    int vsiz;
    const char *vbuf = tcmapget3(mdb->maps[mi], kbuf, ksiz, &vsiz);
    char *rv;
    if (vbuf) {
        TCMEMDUP(rv, vbuf, vsiz);
        *sp = vsiz;
    } else {
        rv = NULL;
    }
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    return rv;
}

/* Initialize the iterator of an on-memory map database object in front of a key. */
void tcmdbiterinit2(TCMDB *mdb, const void *kbuf, int ksiz) {
    if (pthread_mutex_lock(mdb->imtx) != 0) return;
    unsigned int mi;
    TCMDBHASH(mi, kbuf, ksiz);
    if (pthread_rwlock_rdlock((pthread_rwlock_t *) mdb->mmtxs + mi) != 0) {
        pthread_mutex_unlock(mdb->imtx);
        return;
    }
    int vsiz;
    if (tcmapget(mdb->maps[mi], kbuf, ksiz, &vsiz)) {
        for (int i = 0; i < TCMDBMNUM; i++) {
            tcmapiterinit(mdb->maps[i]);
        }
        tcmapiterinit2(mdb->maps[mi], kbuf, ksiz);
        mdb->iter = mi;
    }
    pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + mi);
    pthread_mutex_unlock(mdb->imtx);
}

/* Initialize the iterator of an on-memory map database object in front of a key string. */
void tcmdbiterinit3(TCMDB *mdb, const char *kstr) {
    assert(mdb && kstr);
    tcmdbiterinit2(mdb, kstr, strlen(kstr));
}

/* Process each record atomically of an on-memory hash database object. */
void tcmdbforeach(TCMDB *mdb, TCITER iter, void *op) {
    assert(mdb && iter);
    for (int i = 0; i < TCMDBMNUM; i++) {
        if (pthread_rwlock_wrlock((pthread_rwlock_t *) mdb->mmtxs + i) != 0) {
            while (i >= 0) {
                pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + i);
                i--;
            }
            return;
        }
    }
    bool cont = true;
    for (int i = 0; cont && i < TCMDBMNUM; i++) {
        TCMAP *map = mdb->maps[i];
        TCMAPREC *cur = map->cur;
        tcmapiterinit(map);
        int ksiz;
        const char *kbuf;
        while (cont && (kbuf = tcmapiternext(map, &ksiz)) != NULL) {
            int vsiz;
            const char *vbuf = tcmapiterval(kbuf, &vsiz);
            if (!iter(kbuf, ksiz, vbuf, vsiz, op)) cont = false;
        }
        map->cur = cur;
    }
    for (int i = TCMDBMNUM - 1; i >= 0; i--) {
        pthread_rwlock_unlock((pthread_rwlock_t *) mdb->mmtxs + i);
    }
}



/*************************************************************************************************
 * on-memory tree database
 *************************************************************************************************/

/* Create an on-memory tree database object. */
TCNDB *tcndbnew(void) {
    return tcndbnew2(tccmplexical, NULL);
}

/* Create an on-memory tree database object with specifying the custom comparison function. */
TCNDB *tcndbnew2(TCCMP cmp, void *cmpop) {
    assert(cmp);
    TCNDB *ndb;
    TCMALLOC(ndb, sizeof (*ndb));
    TCMALLOC(ndb->mmtx, sizeof (pthread_mutex_t));
    if (pthread_mutex_init(ndb->mmtx, NULL) != 0) tcmyfatal("mutex error");
    ndb->tree = tctreenew2(cmp, cmpop);
    return ndb;
}

/* Delete an on-memory tree database object. */
void tcndbdel(TCNDB *ndb) {
    assert(ndb);
    tctreedel(ndb->tree);
    pthread_mutex_destroy(ndb->mmtx);
    TCFREE(ndb->mmtx);
    TCFREE(ndb);
}

/* Store a record into an on-memory tree database object. */
void tcndbput(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeput(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Store a string record into an on-memory tree database object. */
void tcndbput2(TCNDB *ndb, const char *kstr, const char *vstr) {
    assert(ndb && kstr && vstr);
    tcndbput(ndb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a new record into an on-memory tree database object. */
bool tcndbputkeep(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return false;
    bool rv = tctreeputkeep(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Store a new string record into an on-memory tree database object. */
bool tcndbputkeep2(TCNDB *ndb, const char *kstr, const char *vstr) {
    assert(ndb && kstr && vstr);
    return tcndbputkeep(ndb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Concatenate a value at the end of the existing record in an on-memory tree database. */
void tcndbputcat(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeputcat(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Concatenate a string at the end of the existing record in an on-memory tree database. */
void tcndbputcat2(TCNDB *ndb, const char *kstr, const char *vstr) {
    assert(ndb && kstr && vstr);
    tcndbputcat(ndb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Remove a record of an on-memory tree database object. */
bool tcndbout(TCNDB *ndb, const void *kbuf, int ksiz) {
    assert(ndb && kbuf && ksiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return false;
    bool rv = tctreeout(ndb->tree, kbuf, ksiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Remove a string record of an on-memory tree database object. */
bool tcndbout2(TCNDB *ndb, const char *kstr) {
    assert(ndb && kstr);
    return tcndbout(ndb, kstr, strlen(kstr));
}

/* Retrieve a record in an on-memory tree database object. */
void *tcndbget(TCNDB *ndb, const void *kbuf, int ksiz, int *sp) {
    assert(ndb && kbuf && ksiz >= 0 && sp);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return NULL;
    int vsiz;
    const char *vbuf = tctreeget(ndb->tree, kbuf, ksiz, &vsiz);
    char *rv;
    if (vbuf) {
        TCMEMDUP(rv, vbuf, vsiz);
        *sp = vsiz;
    } else {
        rv = NULL;
    }
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Retrieve a string record in an on-memory tree database object. */
char *tcndbget2(TCNDB *ndb, const char *kstr) {
    assert(ndb && kstr);
    int vsiz;
    return tcndbget(ndb, kstr, strlen(kstr), &vsiz);
}

/* Get the size of the value of a record in an on-memory tree database object. */
int tcndbvsiz(TCNDB *ndb, const void *kbuf, int ksiz) {
    assert(ndb && kbuf && ksiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return -1;
    int vsiz;
    const char *vbuf = tctreeget(ndb->tree, kbuf, ksiz, &vsiz);
    if (!vbuf) vsiz = -1;
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return vsiz;
}

/* Get the size of the value of a string record in an on-memory tree database object. */
int tcndbvsiz2(TCNDB *ndb, const char *kstr) {
    assert(ndb && kstr);
    return tcndbvsiz(ndb, kstr, strlen(kstr));
}

/* Initialize the iterator of an on-memory tree database object. */
void tcndbiterinit(TCNDB *ndb) {
    assert(ndb);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeiterinit(ndb->tree);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Get the next key of the iterator of an on-memory tree database object. */
void *tcndbiternext(TCNDB *ndb, int *sp) {
    assert(ndb && sp);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return NULL;
    int ksiz;
    const char *kbuf = tctreeiternext(ndb->tree, &ksiz);
    char *rv;
    if (kbuf) {
        TCMEMDUP(rv, kbuf, ksiz);
        *sp = ksiz;
    } else {
        rv = NULL;
    }
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Get the next key string of the iterator of an on-memory tree database object. */
char *tcndbiternext2(TCNDB *ndb) {
    assert(ndb);
    int ksiz;
    return tcndbiternext(ndb, &ksiz);
}

/* Get forward matching keys in an on-memory tree database object. */
TCLIST *tcndbfwmkeys(TCNDB *ndb, const void *pbuf, int psiz, int max) {
    assert(ndb && pbuf && psiz >= 0);
    TCLIST* keys = tclistnew();
    if (pthread_mutex_lock(ndb->mmtx) != 0) return keys;
    if (max < 0) max = INT_MAX;
    TCTREE *tree = ndb->tree;
    TCTREEREC *cur = tree->cur;
    tctreeiterinit2(tree, pbuf, psiz);
    const char *lbuf = NULL;
    int lsiz = 0;
    const char *kbuf;
    int ksiz;
    while (TCLISTNUM(keys) < max && (kbuf = tctreeiternext(tree, &ksiz)) != NULL) {
        if (ksiz < psiz || memcmp(kbuf, pbuf, psiz)) break;
        if (!lbuf || lsiz != ksiz || memcmp(kbuf, lbuf, ksiz)) {
            TCLISTPUSH(keys, kbuf, ksiz);
            if (TCLISTNUM(keys) >= max) break;
            lbuf = kbuf;
            lsiz = ksiz;
        }
    }
    tree->cur = cur;
    pthread_mutex_unlock(ndb->mmtx);
    return keys;
}

/* Get forward matching string keys in an on-memory tree database object. */
TCLIST *tcndbfwmkeys2(TCNDB *ndb, const char *pstr, int max) {
    assert(ndb && pstr);
    return tcndbfwmkeys(ndb, pstr, strlen(pstr), max);
}

/* Get the number of records stored in an on-memory tree database object. */
uint64_t tcndbrnum(TCNDB *ndb) {
    assert(ndb);
    return tctreernum(ndb->tree);
}

/* Get the total size of memory used in an on-memory tree database object. */
uint64_t tcndbmsiz(TCNDB *ndb) {
    assert(ndb);
    return tctreemsiz(ndb->tree);
}

/* Add an integer to a record in an on-memory tree database object. */
int tcndbaddint(TCNDB *ndb, const void *kbuf, int ksiz, int num) {
    assert(ndb && kbuf && ksiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return INT_MIN;
    int rv = tctreeaddint(ndb->tree, kbuf, ksiz, num);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Add a real number to a record in an on-memory tree database object. */
double tcndbadddouble(TCNDB *ndb, const void *kbuf, int ksiz, double num) {
    assert(ndb && kbuf && ksiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return nan("");
    double rv = tctreeadddouble(ndb->tree, kbuf, ksiz, num);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Clear an on-memory tree database object. */
void tcndbvanish(TCNDB *ndb) {
    assert(ndb);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeclear(ndb->tree);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Remove fringe records of an on-memory tree database object. */
void tcndbcutfringe(TCNDB *ndb, int num) {
    assert(ndb && num >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreecutfringe(ndb->tree, num);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}



/*************************************************************************************************
 * ordered tree (for experts)
 *************************************************************************************************/

/* Store a record into a on-memory tree database without balancing nodes. */
void tcndbput3(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeput3(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Store a new record into a on-memory tree database object without balancing nodes. */
bool tcndbputkeep3(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return false;
    bool rv = tctreeputkeep3(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Concatenate a value in a on-memory tree database without balancing nodes. */
void tcndbputcat3(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(ndb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeputcat3(ndb->tree, kbuf, ksiz, vbuf, vsiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Store a record into a on-memory tree database object with a duplication handler. */
bool tcndbputproc(TCNDB *ndb, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
        TCPDPROC proc, void *op) {
    assert(ndb && kbuf && ksiz >= 0 && proc);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return false;
    bool rv = tctreeputproc(ndb->tree, kbuf, ksiz, vbuf, vsiz, proc, op);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Retrieve a record in an on-memory tree database object without balancing nodes. */
void *tcndbget3(TCNDB *ndb, const void *kbuf, int ksiz, int *sp) {
    assert(ndb && kbuf && ksiz >= 0 && sp);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return NULL;
    int vsiz;
    const char *vbuf = tctreeget3(ndb->tree, kbuf, ksiz, &vsiz);
    char *rv;
    if (vbuf) {
        TCMEMDUP(rv, vbuf, vsiz);
        *sp = vsiz;
    } else {
        rv = NULL;
    }
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
    return rv;
}

/* Initialize the iterator of an on-memory tree database object in front of a key. */
void tcndbiterinit2(TCNDB *ndb, const void *kbuf, int ksiz) {
    assert(ndb && kbuf && ksiz >= 0);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    tctreeiterinit2(ndb->tree, kbuf, ksiz);
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}

/* Initialize the iterator of an on-memory tree database object in front of a key string. */
void tcndbiterinit3(TCNDB *ndb, const char *kstr) {
    assert(ndb && kstr);
    tcndbiterinit2(ndb, kstr, strlen(kstr));
}

/* Process each record atomically of an on-memory tree database object. */
void tcndbforeach(TCNDB *ndb, TCITER iter, void *op) {
    assert(ndb && iter);
    if (pthread_mutex_lock((pthread_mutex_t *) ndb->mmtx) != 0) return;
    TCTREE *tree = ndb->tree;
    TCTREEREC *cur = tree->cur;
    tctreeiterinit(tree);
    int ksiz;
    const char *kbuf;
    while ((kbuf = tctreeiternext(tree, &ksiz)) != NULL) {
        int vsiz;
        const char *vbuf = tctreeiterval(kbuf, &vsiz);
        if (!iter(kbuf, ksiz, vbuf, vsiz, op)) break;
    }
    tree->cur = cur;
    pthread_mutex_unlock((pthread_mutex_t *) ndb->mmtx);
}



/*************************************************************************************************
 * memory pool
 *************************************************************************************************/


#define TCMPOOLUNIT    128               // allocation unit size of memory pool elements


/* Global memory pool object. */
TCMPOOL *tcglobalmemorypool = NULL;


/* private function prototypes */
static void tcmpooldelglobal(void);

/* Create a memory pool object. */
TCMPOOL *tcmpoolnew(void) {
    TCMPOOL *mpool;
    TCMALLOC(mpool, sizeof (*mpool));
    TCMALLOC(mpool->mutex, sizeof (pthread_mutex_t));
    if (pthread_mutex_init(mpool->mutex, NULL) != 0) tcmyfatal("locking failed");
    mpool->anum = TCMPOOLUNIT;
    TCMALLOC(mpool->elems, sizeof (mpool->elems[0]) * mpool->anum);
    mpool->num = 0;
    return mpool;
}

/* Delete a memory pool object. */
void tcmpooldel(TCMPOOL *mpool) {
    assert(mpool);
    TCMPELEM *elems = mpool->elems;
    for (int i = mpool->num - 1; i >= 0; i--) {
        elems[i].del(elems[i].ptr);
    }
    TCFREE(elems);
    pthread_mutex_destroy(mpool->mutex);
    TCFREE(mpool->mutex);
    TCFREE(mpool);
}

/* Relegate an arbitrary object to a memory pool object. */
void *tcmpoolpush(TCMPOOL *mpool, void *ptr, void (*del)(void *)) {
    assert(mpool && del);
    if (!ptr) return NULL;
    if (pthread_mutex_lock(mpool->mutex) != 0) tcmyfatal("locking failed");
    int num = mpool->num;
    if (num >= mpool->anum) {
        mpool->anum *= 2;
        TCREALLOC(mpool->elems, mpool->elems, mpool->anum * sizeof (mpool->elems[0]));
    }
    mpool->elems[num].ptr = ptr;
    mpool->elems[num].del = del;
    mpool->num++;
    pthread_mutex_unlock(mpool->mutex);
    return ptr;
}

/* Relegate an allocated region to a memory pool object. */
void *tcmpoolpushptr(TCMPOOL *mpool, void *ptr) {
    assert(mpool);
    return tcmpoolpush(mpool, ptr, (void (*)(void *))free);
}

/* Relegate an extensible string object to a memory pool object. */
TCXSTR *tcmpoolpushxstr(TCMPOOL *mpool, TCXSTR *xstr) {
    assert(mpool);
    return tcmpoolpush(mpool, xstr, (void (*)(void *))tcxstrdel);
}

/* Relegate a list object to a memory pool object. */
TCLIST *tcmpoolpushlist(TCMPOOL *mpool, TCLIST *list) {
    assert(mpool);
    return tcmpoolpush(mpool, list, (void (*)(void *))tclistdel);
}

/* Relegate a map object to a memory pool object. */
TCMAP *tcmpoolpushmap(TCMPOOL *mpool, TCMAP *map) {
    assert(mpool);
    return tcmpoolpush(mpool, map, (void (*)(void *))tcmapdel);
}

/* Relegate a tree object to a memory pool object. */
TCTREE *tcmpoolpushtree(TCMPOOL *mpool, TCTREE *tree) {
    assert(mpool);
    return tcmpoolpush(mpool, tree, (void (*)(void *))tctreedel);
}

/* Allocate a region relegated to a memory pool object. */
void *tcmpoolmalloc(TCMPOOL *mpool, size_t size) {
    assert(mpool && size > 0);
    void *ptr;
    TCMALLOC(ptr, size);
    tcmpoolpush(mpool, ptr, (void (*)(void *))free);
    return ptr;
}

/* Create an extensible string object relegated to a memory pool object. */
TCXSTR *tcmpoolxstrnew(TCMPOOL *mpool) {
    assert(mpool);
    TCXSTR *xstr = tcxstrnew();
    tcmpoolpush(mpool, xstr, (void (*)(void *))tcxstrdel);
    return xstr;
}

/* Create a list object relegated to a memory pool object. */
TCLIST *tcmpoollistnew(TCMPOOL *mpool) {
    assert(mpool);
    TCLIST *list = tclistnew();
    tcmpoolpush(mpool, list, (void (*)(void *))tclistdel);
    return list;
}

/* Create a map object relegated to a memory pool object. */
TCMAP *tcmpoolmapnew(TCMPOOL *mpool) {
    assert(mpool);
    TCMAP *map = tcmapnew();
    tcmpoolpush(mpool, map, (void (*)(void *))tcmapdel);
    return map;
}

/* Create a tree object relegated to a memory pool object. */
TCTREE *tcmpooltreenew(TCMPOOL *mpool) {
    assert(mpool);
    TCTREE *tree = tctreenew();
    tcmpoolpush(mpool, tree, (void (*)(void *))tctreedel);
    return tree;
}

/* Remove the most recently installed cleanup handler of a memory pool object. */
void tcmpoolpop(TCMPOOL *mpool, bool exe) {
    assert(mpool);
    if (pthread_mutex_lock(mpool->mutex) != 0) tcmyfatal("locking failed");
    if (mpool->num > 0) {
        mpool->num--;
        if (exe) mpool->elems[mpool->num].del(mpool->elems[mpool->num].ptr);
    }
    pthread_mutex_unlock(mpool->mutex);
}

/* Remove all cleanup handler of a memory pool object.
   `mpool' specifies the memory pool object. */
void tcmpoolclear(TCMPOOL *mpool, bool exe) {
    assert(mpool);
    if (pthread_mutex_lock(mpool->mutex) != 0) tcmyfatal("locking failed");
    if (exe) {
        for (int i = mpool->num - 1; i >= 0; i--) {
            mpool->elems[i].del(mpool->elems[i].ptr);
        }
    }
    mpool->num = 0;
    pthread_mutex_unlock(mpool->mutex);
}

/* Get the global memory pool object. */
TCMPOOL *tcmpoolglobal(void) {
    if (tcglobalmemorypool) return tcglobalmemorypool;
    tcglobalmemorypool = tcmpoolnew();
    atexit(tcmpooldelglobal);
    return tcglobalmemorypool;
}

/* Detete the global memory pool object. */
static void tcmpooldelglobal(void) {
    if (tcglobalmemorypool) tcmpooldel(tcglobalmemorypool);
}



/*************************************************************************************************
 * miscellaneous utilities
 *************************************************************************************************/



#define TCDISTMAXLEN   4096              // maximum size of a string for distance checking
#define TCDISTBUFSIZ   16384             // size of a distance buffer
#define TCLDBLCOLMAX   16                // maximum number of columns of the long double

/* private function prototypes */
static void tcrandomfdclose(void);
static time_t tcmkgmtime(struct tm *tm);

/* Get the larger value of two integers. */
long tclmax(long a, long b) {
    return (a > b) ? a : b;
}

/* Get the lesser value of two integers. */
long tclmin(long a, long b) {
    return (a < b) ? a : b;
}

#ifndef _WIN32
#define TCRANDDEV      "/dev/urandom"    // path of the random device file
/* File descriptor of random number generator. */
int tcrandomdevfd = -1;

/* Get a random number as long integer based on uniform distribution. */
unsigned long tclrand(void) {
    static uint32_t cnt = 0;
    static uint64_t seed = 0;
    static uint64_t mask = 0;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    if ((cnt & 0xff) == 0 && pthread_mutex_lock(&mutex) == 0) {
        if (cnt == 0) seed += time(NULL);
        if (tcrandomdevfd == -1 && (tcrandomdevfd = open(TCRANDDEV, O_RDONLY, 00644)) != -1) {
            atexit(tcrandomfdclose);
        }
        if (tcrandomdevfd == -1 || read(tcrandomdevfd, &mask, sizeof (mask)) != sizeof (mask)) {
            double t = tctime();
            uint64_t tmask;
            memcpy(&tmask, &t, tclmin(sizeof (t), sizeof (tmask)));
            mask = (mask << 8) ^ tmask;
        }
        pthread_mutex_unlock(&mutex);
    }
    seed = seed * 123456789012301LL + 211;
    uint64_t num = (mask ^ cnt++) ^ seed;
    return TCSWAB64(num);
}

/* Close the random number generator. */
static void tcrandomfdclose(void) {
    close(tcrandomdevfd);
}
#else
static HCRYPTPROV hcprov = 0;

unsigned long tclrand(void) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static bool acquired = false;
    static uint32_t cnt = 0;
    static uint64_t seed = 0;
    static uint64_t mask = 0;
    uint64_t num = 0;
    if ((cnt & 0xff) == 0 && pthread_mutex_lock(&mutex) == 0) {
        if (cnt == 0) seed += time(NULL);
        if (!hcprov) {
            if (!CryptAcquireContext(&hcprov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
                hcprov = 0;
            }
            if (!acquired) {
                acquired = true;
                atexit(tcrandomfdclose);
            }
            if (!hcprov || !CryptGenRandom(hcprov, sizeof (mask), (PBYTE) & mask)) {
                double t = tctime();
                uint64_t tmask;
                memcpy(&tmask, &t, tclmin(sizeof (t), sizeof (tmask)));
                mask = (mask << 8) ^ tmask;
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    seed = seed * 123456789012301LL + 211;
    num = (mask ^ cnt++) ^ seed;
    return TCSWAB64(num);
}

static void tcrandomfdclose(void) {
    if (hcprov) {
        CryptReleaseContext(hcprov, 0);
        hcprov = 0;
    }
}
#endif

/* Get a random number as double decimal based on uniform distribution. */
double tcdrand(void) {
    double val = tclrand() / (double) ULONG_MAX;
    return val < 1.0 ? val : 0.0;
}

/* Get a random number as double decimal based on normal distribution. */
double tcdrandnd(double avg, double sd) {
    assert(sd >= 0.0);
    return sqrt(-2.0 * log(tcdrand())) * cos(2 * 3.141592653589793 * tcdrand()) * sd + avg;
}

/* Compare two strings with case insensitive evaluation. */
int tcstricmp(const char *astr, const char *bstr) {
    assert(astr && bstr);
    while (*astr != '\0') {
        if (*bstr == '\0') return 1;
        int ac = (*astr >= 'A' && *astr <= 'Z') ? *astr + ('a' - 'A') : *(unsigned char *) astr;
        int bc = (*bstr >= 'A' && *bstr <= 'Z') ? *bstr + ('a' - 'A') : *(unsigned char *) bstr;
        if (ac != bc) return ac - bc;
        astr++;
        bstr++;
    }
    return (*bstr == '\0') ? 0 : -1;
}

/* Check whether a string begins with a key. */
bool tcstrfwm(const char *str, const char *key) {
    assert(str && key);
    while (*key != '\0') {
        if (*str != *key || *str == '\0') return false;
        key++;
        str++;
    }
    return true;
}

/* Check whether a string begins with a key with case insensitive evaluation. */
bool tcstrifwm(const char *str, const char *key) {
    assert(str && key);
    while (*key != '\0') {
        if (*str == '\0') return false;
        int sc = *str;
        if (sc >= 'A' && sc <= 'Z') sc += 'a' - 'A';
        int kc = *key;
        if (kc >= 'A' && kc <= 'Z') kc += 'a' - 'A';
        if (sc != kc) return false;
        key++;
        str++;
    }
    return true;
}

/* Check whether a string ends with a key. */
bool tcstrbwm(const char *str, const char *key) {
    assert(str && key);
    int slen = strlen(str);
    int klen = strlen(key);
    for (int i = 1; i <= klen; i++) {
        if (i > slen || str[slen - i] != key[klen - i]) return false;
    }
    return true;
}

/* Check whether a string ends with a key with case insensitive evaluation. */
bool tcstribwm(const char *str, const char *key) {
    assert(str && key);
    int slen = strlen(str);
    int klen = strlen(key);
    for (int i = 1; i <= klen; i++) {
        if (i > slen) return false;
        int sc = str[slen - i];
        if (sc >= 'A' && sc <= 'Z') sc += 'a' - 'A';
        int kc = key[klen - i];
        if (kc >= 'A' && kc <= 'Z') kc += 'a' - 'A';
        if (sc != kc) return false;
    }
    return true;
}

/* Calculate the edit distance of two strings. */
int tcstrdist(const char *astr, const char *bstr) {
    assert(astr && bstr);
    int alen = tclmin(strlen(astr), TCDISTMAXLEN);
    int blen = tclmin(strlen(bstr), TCDISTMAXLEN);
    int dsiz = blen + 1;
    int tbuf[TCDISTBUFSIZ];
    int *tbl;
    if ((alen + 1) * dsiz < TCDISTBUFSIZ) {
        tbl = tbuf;
    } else {
        TCMALLOC(tbl, (alen + 1) * dsiz * sizeof (*tbl));
    }
    for (int i = 0; i <= alen; i++) {
        tbl[i * dsiz] = i;
    }
    for (int i = 1; i <= blen; i++) {
        tbl[i] = i;
    }
    astr--;
    bstr--;
    for (int i = 1; i <= alen; i++) {
        for (int j = 1; j <= blen; j++) {
            int ac = tbl[(i - 1) * dsiz + j] + 1;
            int bc = tbl[i * dsiz + j - 1] + 1;
            int cc = tbl[(i - 1) * dsiz + j - 1] + (astr[i] != bstr[j]);
            ac = ac < bc ? ac : bc;
            tbl[i * dsiz + j] = ac < cc ? ac : cc;
        }
    }
    int rv = tbl[alen * dsiz + blen];
    if (tbl != tbuf) TCFREE(tbl);
    return rv;
}

/* Calculate the edit distance of two UTF-8 strings. */
int tcstrdistutf(const char *astr, const char *bstr) {
    assert(astr && bstr);
    int alen = strlen(astr);
    uint16_t abuf[TCDISTBUFSIZ];
    uint16_t *aary;
    if (alen < TCDISTBUFSIZ) {
        aary = abuf;
    } else {
        TCMALLOC(aary, alen * sizeof (*aary));
    }
    tcstrutftoucs(astr, aary, &alen);
    int blen = strlen(bstr);
    uint16_t bbuf[TCDISTBUFSIZ];
    uint16_t *bary;
    if (blen < TCDISTBUFSIZ) {
        bary = bbuf;
    } else {
        TCMALLOC(bary, blen * sizeof (*bary));
    }
    tcstrutftoucs(bstr, bary, &blen);
    if (alen > TCDISTMAXLEN) alen = TCDISTMAXLEN;
    if (blen > TCDISTMAXLEN) blen = TCDISTMAXLEN;
    int dsiz = blen + 1;
    int tbuf[TCDISTBUFSIZ];
    int *tbl;
    if ((alen + 1) * dsiz < TCDISTBUFSIZ) {
        tbl = tbuf;
    } else {
        TCMALLOC(tbl, (alen + 1) * dsiz * sizeof (*tbl));
    }
    for (int i = 0; i <= alen; i++) {
        tbl[i * dsiz] = i;
    }
    for (int i = 1; i <= blen; i++) {
        tbl[i] = i;
    }
    aary--;
    bary--;
    for (int i = 1; i <= alen; i++) {
        for (int j = 1; j <= blen; j++) {
            int ac = tbl[(i - 1) * dsiz + j] + 1;
            int bc = tbl[i * dsiz + j - 1] + 1;
            int cc = tbl[(i - 1) * dsiz + j - 1] + (aary[i] != bary[j]);
            ac = ac < bc ? ac : bc;
            tbl[i * dsiz + j] = ac < cc ? ac : cc;
        }
    }
    aary++;
    bary++;
    int rv = tbl[alen * dsiz + blen];
    if (tbl != tbuf) TCFREE(tbl);
    if (bary != bbuf) TCFREE(bary);
    if (aary != abuf) TCFREE(aary);
    return rv;
}

/* Convert the letters of a string into upper case. */
char *tcstrtoupper(char *str) {
    assert(str);
    char *wp = str;
    while (*wp != '\0') {
        if (*wp >= 'a' && *wp <= 'z') *wp -= 'a' - 'A';
        wp++;
    }
    return str;
}

/* Convert the letters of a string into lower case. */
char *tcstrtolower(char *str) {
    assert(str);
    char *wp = str;
    while (*wp != '\0') {
        if (*wp >= 'A' && *wp <= 'Z') *wp += 'a' - 'A';
        wp++;
    }
    return str;
}

/* Cut space characters at head or tail of a string. */
char *tcstrtrim(char *str) {
    assert(str);
    const char *rp = str;
    char *wp = str;
    bool head = true;
    while (*rp != '\0') {
        if (*rp > '\0' && *rp <= ' ') {
            if (!head) *(wp++) = *rp;
        } else {
            *(wp++) = *rp;
            head = false;
        }
        rp++;
    }
    *wp = '\0';
    while (wp > str && wp[-1] > '\0' && wp[-1] <= ' ') {
        *(--wp) = '\0';
    }
    return str;
}

/* Squeeze space characters in a string and trim it. */
char *tcstrsqzspc(char *str) {
    assert(str);
    char *rp = str;
    char *wp = str;
    bool spc = true;
    while (*rp != '\0') {
        if (*rp > 0 && *rp <= ' ') {
            if (!spc) *(wp++) = *rp;
            spc = true;
        } else {
            *(wp++) = *rp;
            spc = false;
        }
        rp++;
    }
    *wp = '\0';
    for (wp--; wp >= str; wp--) {
        if (*wp > 0 && *wp <= ' ') {
            *wp = '\0';
        } else {
            break;
        }
    }
    return str;
}

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 */
static const char trailingBytesForUTF8[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

static bool isvalidutf8seq(const unsigned char *seq, int length) {
    unsigned char a;
    const unsigned char *srcptr = seq + length;
    switch (length) {
        default:
            return false;
            /* Everything else falls through when "true"... */
        case 4:
            if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
        case 3:
            if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
        case 2:
            if ((a = (*--srcptr)) > 0xBF) return false;
            switch (*seq) {
                /* no fall-through in this inner switch */
                case 0xE0:
                    if (a < 0xA0) return false;
                    break;
                case 0xF0:
                    if (a < 0x90) return false;
                    break;
                case 0xF4:
                    if (a > 0x8F) return false;
                    break;
                default:
                    if (a < 0x80) return false;
            }
        case 1:
            if (*seq >= 0x80 && *seq < 0xC2) return false;
            if (*seq > 0xF4) return false;
    }
    return true;
}

/* UTF8 string validation. */
bool tcisvalidutf8str(const char *str, int len) {
    if (!str || len < 1) {
        return false;
    }
    int pos = 0;
    int slen = 1;
    for (; pos < len; ++pos) {
        if (str[pos] == '\0' && pos < len - 1) {
            return false;
        }
    }
    pos = 0;
    const unsigned char *ustr = (const unsigned char *) str;
    while (pos < len) {
        slen = trailingBytesForUTF8[*(ustr + pos)] + 1;
        if ((pos + slen) > len) {
            return false;
        }
        if (!isvalidutf8seq(ustr + pos, slen)) {
            return false;
        }
        pos += slen;
    }
    return true;
}


/* Substitute characters in a string. */
char *tcstrsubchr(char *str, const char *rstr, const char *sstr) {
    assert(str && rstr && sstr);
    int slen = strlen(sstr);
    char *wp = str;
    for (int i = 0; str[i] != '\0'; i++) {
        const char *p = strchr(rstr, str[i]);
        if (p) {
            int idx = p - rstr;
            if (idx < slen) *(wp++) = sstr[idx];
        } else {
            *(wp++) = str[i];
        }
    }
    *wp = '\0';
    return str;
}

/* Count the number of characters in a string of UTF-8. */
int tcstrcntutf(const char *str) {
    assert(str);
    const unsigned char *rp = (unsigned char *) str;
    int cnt = 0;
    while (*rp != '\0') {
        if ((*rp & 0x80) == 0x00 || (*rp & 0xe0) == 0xc0 ||
                (*rp & 0xf0) == 0xe0 || (*rp & 0xf8) == 0xf0) cnt++;
        rp++;
    }
    return cnt;
}

/* Cut a string of UTF-8 at the specified number of characters. */
char *tcstrcututf(char *str, int num) {
    assert(str && num >= 0);
    unsigned char *wp = (unsigned char *) str;
    int cnt = 0;
    while (*wp != '\0') {
        if ((*wp & 0x80) == 0x00 || (*wp & 0xe0) == 0xc0 ||
                (*wp & 0xf0) == 0xe0 || (*wp & 0xf8) == 0xf0) {
            cnt++;
            if (cnt > num) {
                *wp = '\0';
                break;
            }
        }
        wp++;
    }
    return str;
}

/* Convert a UTF-8 string into a UCS-2 array. */
void tcstrutftoucs(const char *str, uint16_t *ary, int *np) {
    assert(str && ary && np);
    const unsigned char *rp = (unsigned char *) str;
    unsigned int wi = 0;
    while (*rp != '\0') {
        int c = *(unsigned char *) rp;
        if (c < 0x80) {
            ary[wi++] = c;
        } else if (c < 0xe0) {
            if (rp[1] >= 0x80) {
                ary[wi++] = ((rp[0] & 0x1f) << 6) | (rp[1] & 0x3f);
                rp++;
            }
        } else if (c < 0xf0) {
            if (rp[1] >= 0x80 && rp[2] >= 0x80) {
                ary[wi++] = ((rp[0] & 0xf) << 12) | ((rp[1] & 0x3f) << 6) | (rp[2] & 0x3f);
                rp += 2;
            }
        }
        rp++;
    }
    *np = wi;
}

/* Convert a UCS-2 array into a UTF-8 string. */
int tcstrucstoutf(const uint16_t *ary, int num, char *str) {
    assert(ary && num >= 0 && str);
    unsigned char *wp = (unsigned char *) str;
    for (int i = 0; i < num; i++) {
        unsigned int c = ary[i];
        if (c < 0x80) {
            *(wp++) = c;
        } else if (c < 0x800) {
            *(wp++) = 0xc0 | (c >> 6);
            *(wp++) = 0x80 | (c & 0x3f);
        } else {
            *(wp++) = 0xe0 | (c >> 12);
            *(wp++) = 0x80 | ((c & 0xfff) >> 6);
            *(wp++) = 0x80 | (c & 0x3f);
        }
    }
    *wp = '\0';
    return (char *) wp - str;
}

/* Create a list object by splitting a string. */
TCLIST *tcstrsplit(const char *str, const char *delims) {
    assert(str && delims);
    TCLIST *list = tclistnew();
    while (true) {
        const char *sp = str;
        while (*str != '\0' && !strchr(delims, *str)) {
            str++;
        }
        TCLISTPUSH(list, sp, str - sp);
        if (*str == '\0') break;
        str++;
    }
    return list;
}

/* Create a string by joining all elements of a list object. */
char *tcstrjoin(const TCLIST *list, char delim) {
    assert(list);
    int num = TCLISTNUM(list);
    int size = num + 1;
    for (int i = 0; i < num; i++) {
        size += TCLISTVALSIZ(list, i);
    }
    char *buf;
    TCMALLOC(buf, size);
    char *wp = buf;
    for (int i = 0; i < num; i++) {
        if (i > 0) *(wp++) = delim;
        int vsiz;
        const char *vbuf = tclistval(list, i, &vsiz);
        memcpy(wp, vbuf, vsiz);
        wp += vsiz;
    }
    *wp = '\0';
    return buf;
}

/* Convert a string to an integer. */
int64_t tcatoi(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    int sign = 1;
    int64_t num = 0;
    if (*str == '-') {
        str++;
        sign = -1;
    } else if (*str == '+') {
        str++;
    }
    if (tcstrifwm(str, "inf")) return (INT64_MAX * sign);
    if (tcstrifwm(str, "nan")) return 0;
    while (*str != '\0') {
        if (*str < '0' || *str > '9') break;
        num = num * 10 + *str - '0';
        str++;
    }
    return num * sign;
}

/* Convert a string with a metric prefix to an integer. */
int64_t tcatoix(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    int sign = 1;
    if (*str == '-') {
        str++;
        sign = -1;
    } else if (*str == '+') {
        str++;
    }
    long double num = 0;
    while (*str != '\0') {
        if (*str < '0' || *str > '9') break;
        num = num * 10 + *str - '0';
        str++;
    }
    if (*str == '.') {
        str++;
        long double base = 10;
        while (*str != '\0') {
            if (*str < '0' || *str > '9') break;
            num += (*str - '0') / base;
            str++;
            base *= 10;
        }
    }
    num *= sign;
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    if (*str == 'k' || *str == 'K') {
        num *= 1LL << 10;
    } else if (*str == 'm' || *str == 'M') {
        num *= 1LL << 20;
    } else if (*str == 'g' || *str == 'G') {
        num *= 1LL << 30;
    } else if (*str == 't' || *str == 'T') {
        num *= 1LL << 40;
    } else if (*str == 'p' || *str == 'P') {
        num *= 1LL << 50;
    } else if (*str == 'e' || *str == 'E') {
        num *= 1LL << 60;
    }
    if (num > INT64_MAX) return INT64_MAX;
    if (num < INT64_MIN) return INT64_MIN;
    return num;
}

/* Convert a string to a real number. */
double tcatof(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    int sign = 1;
    if (*str == '-') {
        str++;
        sign = -1;
    } else if (*str == '+') {
        str++;
    }
    if (tcstrifwm(str, "inf")) return HUGE_VAL * sign;
    if (tcstrifwm(str, "nan")) return nan("");
    long double num = 0;
    int col = 0;
    while (*str != '\0') {
        if (*str < '0' || *str > '9') break;
        num = num * 10 + *str - '0';
        str++;
        if (num > 0) col++;
    }
    if (*str == '.') {
        str++;
        long double fract = 0.0;
        long double base = 10;
        while (col < TCLDBLCOLMAX && *str != '\0') {
            if (*str < '0' || *str > '9') break;
            fract += (*str - '0') / base;
            str++;
            col++;
            base *= 10;
        }
        num += fract;
    }
    if (*str == 'e' || *str == 'E') {
        str++;
        num *= pow(10, tcatoi(str));
    }
    return num * sign;
}

long double tcatof2(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    int sign = 1;
    if (*str == '-') {
        str++;
        sign = -1;
    } else if (*str == '+') {
        str++;
    }
    if (tcstrifwm(str, "inf")) return HUGE_VALL * sign;
    if (tcstrifwm(str, "nan")) return nan("");
    long double num = 0;
    int col = 0;
    while (*str != '\0') {
        if (*str < '0' || *str > '9') break;
        num = num * 10 + *str - '0';
        str++;
        if (num > 0) col++;
    }
    if (*str == '.') {
        str++;
        long double fract = 0.0;
        long double base = 10;
        while (col < TCLDBLCOLMAX && *str != '\0') {
            if (*str < '0' || *str > '9') break;
            fract += (*str - '0') / base;
            str++;
            col++;
            base *= 10;
        }
        num += fract;
    }
    if (*str == 'e' || *str == 'E') {
        str++;
        num *= pow(10, tcatoi(str));
    }
    return num * sign;
}


//Basic code of `ftoa()' taken from scmRTOS https://sourceforge.net/projects/scmrtos
//Copyright (c) 2009-2012by Anton Gusev aka AHTOXA
#define FTOA_MAX_PRECISION	(10)
static const double rounders[FTOA_MAX_PRECISION + 1] = {
    0.5, // 0
    0.05, // 1
    0.005, // 2
    0.0005, // 3
    0.00005, // 4
    0.000005, // 5
    0.0000005, // 6
    0.00000005, // 7
    0.000000005, // 8
    0.0000000005, // 9
    0.00000000005 // 10
};

int tcftoa(long double f, char *buf, int max, int precision) {

#define FTOA_SZSTEP(_step) if ((ret += (_step)) >= max) { \
                                *ptr = 0; \
                                return ret; \
                           }
    if (max <= 0) {
        return 0;
    }
    char *ptr = buf;
    char *p = ptr;
    char c;
    char *p1;
    long intPart;
    int ret = 0;

    // check precision bounds
    if (precision > FTOA_MAX_PRECISION)
        precision = FTOA_MAX_PRECISION;

    // sign stuff
    if (f < 0) {
        f = -f;
        FTOA_SZSTEP(1)
                *ptr++ = '-';
    }
    if (precision == -1) {
        if (f < 1.0) precision = 6;
        else if (f < 10.0) precision = 5;
        else if (f < 100.0) precision = 4;
        else if (f < 1000.0) precision = 3;
        else if (f < 10000.0) precision = 2;
        else if (f < 100000.0) precision = 1;
        else precision = 0;
    }
    if (precision) {
        // round value according the precision
        f += rounders[precision];
    }
    // integer part...
    intPart = f;
    f -= intPart;

    if (!intPart) {
        FTOA_SZSTEP(1)
                *ptr++ = '0';
    } else {
        // save start pointer
        p = ptr;
        while (intPart) {
            if (++ret >= max) { //overflow condition
                memmove(ptr, ptr + 1, p - ptr);
                p--;
            }
            *p++ = '0' + intPart % 10;
            intPart /= 10;
        }
        // save end pos
        p1 = p;
        // reverse result
        while (p > ptr) {
            c = *--p;
            *p = *ptr;
            *ptr++ = c;
        }
        if (ret >= max) {
            ptr = p1;
            *ptr = 0;
            return ret;
        }
        // restore end pos
        ptr = p1;
    }

    // decimal part
    if (precision) {
        // place decimal point
        if ((ret += 1) + 1 >= max) { //reserve one more after dot
            *ptr = 0;
            return ret;
        }
        *ptr++ = '.';
        // convert
        while (precision--) {
            f *= 10.0;
            c = f;
            FTOA_SZSTEP(1)
                    *ptr++ = '0' + c;
            f -= c;
        }
    }
    // terminating zero
    *ptr = 0;
    return ret;

#undef FTOA_SZSTEP
}

/* Check whether a string matches a regular expression. */
bool tcregexmatch(const char *str, const char *regex) {
    assert(str && regex);
    int options = REG_EXTENDED | REG_NOSUB;
    if (*regex == '*') {
        options |= REG_ICASE;
        regex++;
    }
    regex_t rbuf;
    if (regcomp(&rbuf, regex, options) != 0) return false;
    bool rv = regexec(&rbuf, str, 0, NULL, 0) == 0;
    regfree(&rbuf);
    return rv;
}

/* Replace each substring matching a regular expression string. */
char *tcregexreplace(const char *str, const char *regex, const char *alt) {
    assert(str && regex && alt);
    int options = REG_EXTENDED;
    if (*regex == '*') {
        options |= REG_ICASE;
        regex++;
    }
    regex_t rbuf;
    if (regex[0] == '\0' || regcomp(&rbuf, regex, options) != 0) return tcstrdup(str);
    regmatch_t subs[256];
    if (regexec(&rbuf, str, 32, subs, 0) != 0) {
        regfree(&rbuf);
        return tcstrdup(str);
    }
    const char *sp = str;
    TCXSTR *xstr = tcxstrnew();
    bool first = true;
    while (sp[0] != '\0' && regexec(&rbuf, sp, 10, subs, first ? 0 : REG_NOTBOL) == 0) {
        first = false;
        if (subs[0].rm_so == -1) break;
        tcxstrcat(xstr, sp, subs[0].rm_so);
        for (const char *rp = alt; *rp != '\0'; rp++) {
            if (*rp == '\\') {
                if (rp[1] >= '0' && rp[1] <= '9') {
                    int num = rp[1] - '0';
                    if (subs[num].rm_so != -1 && subs[num].rm_eo != -1)
                        tcxstrcat(xstr, sp + subs[num].rm_so, subs[num].rm_eo - subs[num].rm_so);
                    ++rp;
                } else if (rp[1] != '\0') {
                    tcxstrcat(xstr, ++rp, 1);
                }
            } else if (*rp == '&') {
                tcxstrcat(xstr, sp + subs[0].rm_so, subs[0].rm_eo - subs[0].rm_so);
            } else {
                tcxstrcat(xstr, rp, 1);
            }
        }
        sp += subs[0].rm_eo;
        if (subs[0].rm_eo < 1) break;
    }
    tcxstrcat2(xstr, sp);
    regfree(&rbuf);
    return tcxstrtomalloc(xstr);
}

/* Get the MD5 hash value of a serial object. */
void tcmd5hash(const void *ptr, int size, char *buf) {
    assert(ptr && size >= 0 && buf);
    int i;
    md5_state_t ms;
    md5_init(&ms);
    md5_append(&ms, (md5_byte_t *) ptr, size);
    unsigned char digest[16];
    md5_finish(&ms, (md5_byte_t *) digest);
    char *wp = buf;
    for (i = 0; i < 16; i++) {
        wp += sprintf(wp, "%02x", digest[i]);
    }
    *wp = '\0';
}

/* Cipher or decipher a serial object with the Arcfour stream cipher. */
void tcarccipher(const void *ptr, int size, const void *kbuf, int ksiz, void *obuf) {
    assert(ptr && size >= 0 && kbuf && ksiz >= 0 && obuf);
    if (ksiz < 1) {
        kbuf = "";
        ksiz = 1;
    }
    uint32_t sbox[0x100], kbox[0x100];
    for (int i = 0; i < 0x100; i++) {
        sbox[i] = i;
        kbox[i] = ((uint8_t *) kbuf)[i % ksiz];
    }
    int sidx = 0;
    for (int i = 0; i < 0x100; i++) {
        sidx = (sidx + sbox[i] + kbox[i]) & 0xff;
        uint32_t swap = sbox[i];
        sbox[i] = sbox[sidx];
        sbox[sidx] = swap;
    }
    int x = 0;
    int y = 0;
    for (int i = 0; i < size; i++) {
        x = (x + 1) & 0xff;
        y = (y + sbox[x]) & 0xff;
        int32_t swap = sbox[x];
        sbox[x] = sbox[y];
        sbox[y] = swap;
        ((uint8_t *) obuf)[i] = ((uint8_t *) ptr)[i] ^ sbox[(sbox[x] + sbox[y])&0xff];
    }
}

/* Get the time of day in seconds. */
double tctime(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) return 0.0;
    return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

/* Get time in milleconds since Epoch. */
unsigned long tcmstime(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) return 0;
    return (unsigned long long) (tv.tv_sec) * 1000 + (unsigned long long) (tv.tv_usec) / 1000;
}

/* Get the Gregorian calendar of a time. */
void tccalendar(int64_t t, int jl, int *yearp, int *monp, int *dayp,
        int *hourp, int *minp, int *secp) {
    if (t == INT64_MAX) t = time(NULL);
    if (jl == INT_MAX) jl = tcjetlag();
    time_t tt = (time_t) t + jl;
    struct tm ts;
    if (!gmtime_r(&tt, &ts)) {
        if (yearp) *yearp = 0;
        if (monp) *monp = 0;
        if (dayp) *dayp = 0;
        if (hourp) *hourp = 0;
        if (minp) *minp = 0;
        if (secp) *secp = 0;
    } else {
        if (yearp) *yearp = ts.tm_year + 1900;
        if (monp) *monp = ts.tm_mon + 1;
        if (dayp) *dayp = ts.tm_mday;
        if (hourp) *hourp = ts.tm_hour;
        if (minp) *minp = ts.tm_min;
        if (secp) *secp = ts.tm_sec;
    }
}

/* Format a date as a string in W3CDTF. */
void tcdatestrwww(int64_t t, int jl, char *buf) {
    assert(buf);
    if (t == INT64_MAX) t = time(NULL);
    if (jl == INT_MAX) jl = tcjetlag();
    time_t tt = (time_t) t + jl;
    struct tm ts;
    if (!gmtime_r(&tt, &ts)) memset(&ts, 0, sizeof (ts));
    ts.tm_year += 1900;
    ts.tm_mon += 1;
    jl /= 60;
    char tzone[16];
    if (jl == 0) {
        sprintf(tzone, "Z");
    } else if (jl < 0) {
        jl *= -1;
        sprintf(tzone, "-%02d:%02d", jl / 60, jl % 60);
    } else {
        sprintf(tzone, "+%02d:%02d", jl / 60, jl % 60);
    }
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d%s",
            ts.tm_year, ts.tm_mon, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, tzone);
}

/* Format a date as a string in RFC 1123 format. */
void tcdatestrhttp(int64_t t, int jl, char *buf) {
    assert(buf);
    if (t == INT64_MAX) t = time(NULL);
    if (jl == INT_MAX) jl = tcjetlag();
    time_t tt = (time_t) t + jl;
    struct tm ts;
    if (!gmtime_r(&tt, &ts)) memset(&ts, 0, sizeof (ts));
    ts.tm_year += 1900;
    ts.tm_mon += 1;
    jl /= 60;
    char *wp = buf;
    switch (tcdayofweek(ts.tm_year, ts.tm_mon, ts.tm_mday)) {
        case 0: wp += sprintf(wp, "Sun, ");
            break;
        case 1: wp += sprintf(wp, "Mon, ");
            break;
        case 2: wp += sprintf(wp, "Tue, ");
            break;
        case 3: wp += sprintf(wp, "Wed, ");
            break;
        case 4: wp += sprintf(wp, "Thu, ");
            break;
        case 5: wp += sprintf(wp, "Fri, ");
            break;
        case 6: wp += sprintf(wp, "Sat, ");
            break;
    }
    wp += sprintf(wp, "%02d ", ts.tm_mday);
    switch (ts.tm_mon) {
        case 1: wp += sprintf(wp, "Jan ");
            break;
        case 2: wp += sprintf(wp, "Feb ");
            break;
        case 3: wp += sprintf(wp, "Mar ");
            break;
        case 4: wp += sprintf(wp, "Apr ");
            break;
        case 5: wp += sprintf(wp, "May ");
            break;
        case 6: wp += sprintf(wp, "Jun ");
            break;
        case 7: wp += sprintf(wp, "Jul ");
            break;
        case 8: wp += sprintf(wp, "Aug ");
            break;
        case 9: wp += sprintf(wp, "Sep ");
            break;
        case 10: wp += sprintf(wp, "Oct ");
            break;
        case 11: wp += sprintf(wp, "Nov ");
            break;
        case 12: wp += sprintf(wp, "Dec ");
            break;
    }
    wp += sprintf(wp, "%04d %02d:%02d:%02d ", ts.tm_year, ts.tm_hour, ts.tm_min, ts.tm_sec);
    if (jl == 0) {
        sprintf(wp, "GMT");
    } else if (jl < 0) {
        jl *= -1;
        sprintf(wp, "-%02d%02d", jl / 60, jl % 60);
    } else {
        sprintf(wp, "+%02d%02d", jl / 60, jl % 60);
    }
}

/* Get the time value of a date string. */
int64_t tcstrmktime(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    if (*str == '\0') return INT64_MIN;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) return tcatoih(str + 2);
    struct tm ts;
    memset(&ts, 0, sizeof (ts));
    ts.tm_year = 70;
    ts.tm_mon = 0;
    ts.tm_mday = 1;
    ts.tm_hour = 0;
    ts.tm_min = 0;
    ts.tm_sec = 0;
    ts.tm_isdst = 0;
    int len = strlen(str);
    time_t t = (time_t) tcatoi(str);
    const char *pv = str;
    while (*pv >= '0' && *pv <= '9') {
        pv++;
    }
    while (*pv > '\0' && *pv <= ' ') {
        pv++;
    }
    if (*pv == '\0') return (int64_t) t;
    if ((pv[0] == 's' || pv[0] == 'S') && pv[1] >= '\0' && pv[1] <= ' ')
        return (int64_t) t;
    if ((pv[0] == 'm' || pv[0] == 'M') && pv[1] >= '\0' && pv[1] <= ' ')
        return (int64_t) t * 60;
    if ((pv[0] == 'h' || pv[0] == 'H') && pv[1] >= '\0' && pv[1] <= ' ')
        return (int64_t) t * 60 * 60;
    if ((pv[0] == 'd' || pv[0] == 'D') && pv[1] >= '\0' && pv[1] <= ' ')
        return (int64_t) t * 60 * 60 * 24;
    if (len > 4 && str[4] == '-') {
        ts.tm_year = tcatoi(str) - 1900;
        if ((pv = strchr(str, '-')) != NULL && pv - str == 4) {
            const char *rp = pv + 1;
            ts.tm_mon = tcatoi(rp) - 1;
            if ((pv = strchr(rp, '-')) != NULL && pv - str == 7) {
                rp = pv + 1;
                ts.tm_mday = tcatoi(rp);
                if ((pv = strchr(rp, 'T')) != NULL && pv - str == 10) {
                    rp = pv + 1;
                    ts.tm_hour = tcatoi(rp);
                    if ((pv = strchr(rp, ':')) != NULL && pv - str == 13) {
                        rp = pv + 1;
                        ts.tm_min = tcatoi(rp);
                    }
                    if ((pv = strchr(rp, ':')) != NULL && pv - str == 16) {
                        rp = pv + 1;
                        ts.tm_sec = tcatoi(rp);
                    }
                    if ((pv = strchr(rp, '.')) != NULL && pv - str >= 19) rp = pv + 1;
                    pv = rp;
                    while (*pv >= '0' && *pv <= '9') {
                        pv++;
                    }
                    if ((*pv == '+' || *pv == '-') && strlen(pv) >= 6 && pv[3] == ':')
                        ts.tm_sec -= (tcatoi(pv + 1) * 3600 + tcatoi(pv + 4) * 60) * (pv[0] == '+' ? 1 : -1);
                }
            }
        }
        return (int64_t) tcmkgmtime(&ts);
    }
    if (len > 4 && str[4] == '/') {
        ts.tm_year = tcatoi(str) - 1900;
        if ((pv = strchr(str, '/')) != NULL && pv - str == 4) {
            const char *rp = pv + 1;
            ts.tm_mon = tcatoi(rp) - 1;
            if ((pv = strchr(rp, '/')) != NULL && pv - str == 7) {
                rp = pv + 1;
                ts.tm_mday = tcatoi(rp);
                if ((pv = strchr(rp, ' ')) != NULL && pv - str == 10) {
                    rp = pv + 1;
                    ts.tm_hour = tcatoi(rp);
                    if ((pv = strchr(rp, ':')) != NULL && pv - str == 13) {
                        rp = pv + 1;
                        ts.tm_min = tcatoi(rp);
                    }
                    if ((pv = strchr(rp, ':')) != NULL && pv - str == 16) {
                        rp = pv + 1;
                        ts.tm_sec = tcatoi(rp);
                    }
                    if ((pv = strchr(rp, '.')) != NULL && pv - str >= 19) rp = pv + 1;
                    pv = rp;
                    while (*pv >= '0' && *pv <= '9') {
                        pv++;
                    }
                    if ((*pv == '+' || *pv == '-') && strlen(pv) >= 6 && pv[3] == ':')
                        ts.tm_sec -= (tcatoi(pv + 1) * 3600 + tcatoi(pv + 4) * 60) * (pv[0] == '+' ? 1 : -1);
                }
            }
        }
        return (int64_t) tcmkgmtime(&ts);
    }
    const char *crp = str;
    if (len >= 4 && str[3] == ',') crp = str + 4;
    while (*crp == ' ') {
        crp++;
    }
    ts.tm_mday = tcatoi(crp);
    while ((*crp >= '0' && *crp <= '9') || *crp == ' ') {
        crp++;
    }
    if (tcstrifwm(crp, "Jan")) {
        ts.tm_mon = 0;
    } else if (tcstrifwm(crp, "Feb")) {
        ts.tm_mon = 1;
    } else if (tcstrifwm(crp, "Mar")) {
        ts.tm_mon = 2;
    } else if (tcstrifwm(crp, "Apr")) {
        ts.tm_mon = 3;
    } else if (tcstrifwm(crp, "May")) {
        ts.tm_mon = 4;
    } else if (tcstrifwm(crp, "Jun")) {
        ts.tm_mon = 5;
    } else if (tcstrifwm(crp, "Jul")) {
        ts.tm_mon = 6;
    } else if (tcstrifwm(crp, "Aug")) {
        ts.tm_mon = 7;
    } else if (tcstrifwm(crp, "Sep")) {
        ts.tm_mon = 8;
    } else if (tcstrifwm(crp, "Oct")) {
        ts.tm_mon = 9;
    } else if (tcstrifwm(crp, "Nov")) {
        ts.tm_mon = 10;
    } else if (tcstrifwm(crp, "Dec")) {
        ts.tm_mon = 11;
    } else {
        ts.tm_mon = -1;
    }
    if (ts.tm_mon >= 0) crp += 3;
    while (*crp == ' ') {
        crp++;
    }
    ts.tm_year = tcatoi(crp);
    if (ts.tm_year >= 1969) ts.tm_year -= 1900;
    while (*crp >= '0' && *crp <= '9') {
        crp++;
    }
    while (*crp == ' ') {
        crp++;
    }
    if (ts.tm_mday > 0 && ts.tm_mon >= 0 && ts.tm_year >= 0) {
        int clen = strlen(crp);
        if (clen >= 8 && crp[2] == ':' && crp[5] == ':') {
            ts.tm_hour = tcatoi(crp + 0);
            ts.tm_min = tcatoi(crp + 3);
            ts.tm_sec = tcatoi(crp + 6);
            if (clen >= 14 && crp[8] == ' ' && (crp[9] == '+' || crp[9] == '-')) {
                ts.tm_sec -= ((crp[10] - '0') * 36000 + (crp[11] - '0') * 3600 +
                        (crp[12] - '0') * 600 + (crp[13] - '0') * 60) * (crp[9] == '+' ? 1 : -1);
            } else if (clen > 9) {
                if (!strcmp(crp + 9, "JST")) {
                    ts.tm_sec -= 9 * 3600;
                } else if (!strcmp(crp + 9, "CCT")) {
                    ts.tm_sec -= 8 * 3600;
                } else if (!strcmp(crp + 9, "KST")) {
                    ts.tm_sec -= 9 * 3600;
                } else if (!strcmp(crp + 9, "EDT")) {
                    ts.tm_sec -= -4 * 3600;
                } else if (!strcmp(crp + 9, "EST")) {
                    ts.tm_sec -= -5 * 3600;
                } else if (!strcmp(crp + 9, "CDT")) {
                    ts.tm_sec -= -5 * 3600;
                } else if (!strcmp(crp + 9, "CST")) {
                    ts.tm_sec -= -6 * 3600;
                } else if (!strcmp(crp + 9, "MDT")) {
                    ts.tm_sec -= -6 * 3600;
                } else if (!strcmp(crp + 9, "MST")) {
                    ts.tm_sec -= -7 * 3600;
                } else if (!strcmp(crp + 9, "PDT")) {
                    ts.tm_sec -= -7 * 3600;
                } else if (!strcmp(crp + 9, "PST")) {
                    ts.tm_sec -= -8 * 3600;
                } else if (!strcmp(crp + 9, "HDT")) {
                    ts.tm_sec -= -9 * 3600;
                } else if (!strcmp(crp + 9, "HST")) {
                    ts.tm_sec -= -10 * 3600;
                }
            }
        }
        return (int64_t) tcmkgmtime(&ts);
    }
    return INT64_MIN;
}

/* Get the jet lag of the local time. */
int tcjetlag(void) {
#if defined(_SYS_LINUX_)
    tzset();
    return -timezone;
#else
    time_t t = 86400;
    struct tm gts;
    if (!gmtime_r(&t, &gts)) return 0;
    struct tm lts;
    t = 86400;
    if (!localtime_r(&t, &lts)) return 0;
    return mktime(&lts) - mktime(&gts);
#endif
}

/* Get the day of week of a date. */
int tcdayofweek(int year, int mon, int day) {
    if (mon < 3) {
        year--;
        mon += 12;
    }
    return (day + ((8 + (13 * mon)) / 5) + (year + (year / 4) - (year / 100) + (year / 400))) % 7;
}

/* Make the GMT from a time structure.
   `tm' specifies the pointer to the time structure.
   The return value is the GMT. */
static time_t tcmkgmtime(struct tm *tm) {
#if defined(_SYS_LINUX_)
    assert(tm);
    return timegm(tm);
#else
    assert(tm);
    return mktime(tm) + tcjetlag();
#endif
}



/*************************************************************************************************
 * miscellaneous utilities (for experts)
 *************************************************************************************************/


#define TCCHIDXVNNUM   128               // number of virtual node of consistent hashing


/* private function prototypes */
static int tcstrutfkwicputtext(const uint16_t *oary, const uint16_t *nary, int si, int ti,
        int end, char *buf, const TCLIST *uwords, int opts);
static int tcchidxcmp(const void *a, const void *b);

/* Check whether a string is numeric completely or not. */
bool tcstrisnum(const char *str) {
    assert(str);
    bool isnum = false;
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    if (*str == '-') str++;
    while (*str >= '0' && *str <= '9') {
        isnum = true;
        str++;
    }
    if (*str == '.') str++;
    while (*str >= '0' && *str <= '9') {
        isnum = true;
        str++;
    }
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    return isnum && *str == '\0';
}

bool tcstrisintnum(const char *str, int len) {
    assert(str);
    bool isnum = false;
    while (len > 0 && *str > '\0' && *str <= ' ') {
        str++;
        len--;
    }
    while (len > 0 && *str >= '0' && *str <= '9') {
        isnum = true;
        str++;
        len--;
    }
    while (len > 0 && *str > '\0' && *str <= ' ') {
        str++;
        len--;
    }
    return isnum && (*str == '\0' || len == 0);
}

/* Convert a hexadecimal string to an integer. */
int64_t tcatoih(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    int64_t num = 0;
    while (true) {
        if (*str >= '0' && *str <= '9') {
            num = num * 0x10 + *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            num = num * 0x10 + *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            num = num * 0x10 + *str - 'A' + 10;
        } else {
            break;
        }
        str++;
    }
    return num;
}

/* Skip space characters at head of a string. */
const char *tcstrskipspc(const char *str) {
    assert(str);
    while (*str > '\0' && *str <= ' ') {
        str++;
    }
    return str;
}

/* Normalize a UTF-8 string. */
char *tcstrutfnorm(char *str, int opts) {
    assert(str);
    uint16_t buf[TCDISTBUFSIZ];
    uint16_t *ary;
    int len = strlen(str);
    if (len < TCDISTBUFSIZ) {
        ary = buf;
    } else {
        TCMALLOC(ary, len * sizeof (*ary));
    }
    int num;
    tcstrutftoucs(str, ary, &num);
    num = tcstrucsnorm(ary, num, opts);
    tcstrucstoutf(ary, num, str);
    if (ary != buf) TCFREE(ary);
    return str;
}

/* Normalize a UCS-2 array. */
int tcstrucsnorm(uint16_t *ary, int num, int opts) {
    assert(ary && num >= 0);
    bool spcmode = opts & TCUNSPACE;
    bool lowmode = opts & TCUNLOWER;
    bool nacmode = opts & TCUNNOACC;
    bool widmode = opts & TCUNWIDTH;
    int wi = 0;
    for (int i = 0; i < num; i++) {
        int c = ary[i];
        int high = c >> 8;
        if (high == 0x00) {
            if (c <= 0x0020 || c == 0x007f) {
                // control characters
                if (spcmode) {
                    ary[wi++] = 0x0020;
                    if (wi < 2 || ary[wi - 2] == 0x0020) wi--;
                } else if (c == 0x0009 || c == 0x000a || c == 0x000d) {
                    ary[wi++] = c;
                } else {
                    ary[wi++] = 0x0020;
                }
            } else if (c == 0x00a0) {
                // no-break space
                if (spcmode) {
                    ary[wi++] = 0x0020;
                    if (wi < 2 || ary[wi - 2] == 0x0020) wi--;
                } else {
                    ary[wi++] = c;
                }
            } else {
                // otherwise
                if (lowmode) {
                    if (c < 0x007f) {
                        if (c >= 0x0041 && c <= 0x005a) c += 0x20;
                    } else if (c >= 0x00c0 && c <= 0x00de && c != 0x00d7) {
                        c += 0x20;
                    }
                }
                if (nacmode) {
                    if (c >= 0x00c0 && c <= 0x00c5) {
                        c = 'A';
                    } else if (c == 0x00c7) {
                        c = 'C';
                    }
                    if (c >= 0x00c7 && c <= 0x00cb) {
                        c = 'E';
                    }
                    if (c >= 0x00cc && c <= 0x00cf) {
                        c = 'I';
                    } else if (c == 0x00d0) {
                        c = 'D';
                    } else if (c == 0x00d1) {
                        c = 'N';
                    }
                    if ((c >= 0x00d2 && c <= 0x00d6) || c == 0x00d8) {
                        c = 'O';
                    }
                    if (c >= 0x00d9 && c <= 0x00dc) {
                        c = 'U';
                    }
                    if (c == 0x00dd || c == 0x00de) {
                        c = 'Y';
                    } else if (c == 0x00df) {
                        c = 's';
                    } else if (c >= 0x00e0 && c <= 0x00e5) {
                        c = 'a';
                    } else if (c == 0x00e7) {
                        c = 'c';
                    }
                    if (c >= 0x00e7 && c <= 0x00eb) {
                        c = 'e';
                    }
                    if (c >= 0x00ec && c <= 0x00ef) {
                        c = 'i';
                    } else if (c == 0x00f0) {
                        c = 'd';
                    } else if (c == 0x00f1) {
                        c = 'n';
                    }
                    if ((c >= 0x00f2 && c <= 0x00f6) || c == 0x00f8) {
                        c = 'o';
                    }
                    if (c >= 0x00f9 && c <= 0x00fc) {
                        c = 'u';
                    }
                    if (c >= 0x00fd && c <= 0x00ff) {
                        c = 'y';
                    }
                }
                ary[wi++] = c;
            }
        } else if (high == 0x01) {
            // latin-1 extended
            if (lowmode) {
                if (c <= 0x0137) {
                    if ((c & 1) == 0) c++;
                } else if (c == 0x0138) {
                    c += 0;
                } else if (c <= 0x0148) {
                    if ((c & 1) == 1) c++;
                } else if (c == 0x0149) {
                    c += 0;
                } else if (c <= 0x0177) {
                    if ((c & 1) == 0) c++;
                } else if (c == 0x0178) {
                    c = 0x00ff;
                } else if (c <= 0x017e) {
                    if ((c & 1) == 1) c++;
                } else if (c == 0x017f) {
                    c += 0;
                }
            }
            if (nacmode) {
                if (c == 0x00ff) {
                    c = 'y';
                } else if (c <= 0x0105) {
                    c = ((c & 1) == 0) ? 'A' : 'a';
                } else if (c <= 0x010d) {
                    c = ((c & 1) == 0) ? 'C' : 'c';
                } else if (c <= 0x0111) {
                    c = ((c & 1) == 0) ? 'D' : 'd';
                } else if (c <= 0x011b) {
                    c = ((c & 1) == 0) ? 'E' : 'e';
                } else if (c <= 0x0123) {
                    c = ((c & 1) == 0) ? 'G' : 'g';
                } else if (c <= 0x0127) {
                    c = ((c & 1) == 0) ? 'H' : 'h';
                } else if (c <= 0x0131) {
                    c = ((c & 1) == 0) ? 'I' : 'i';
                } else if (c == 0x0134) {
                    c = 'J';
                } else if (c == 0x0135) {
                    c = 'j';
                } else if (c == 0x0136) {
                    c = 'K';
                } else if (c == 0x0137) {
                    c = 'k';
                } else if (c == 0x0138) {
                    c = 'k';
                } else if (c >= 0x0139 && c <= 0x0142) {
                    c = ((c & 1) == 1) ? 'L' : 'l';
                } else if (c >= 0x0143 && c <= 0x0148) {
                    c = ((c & 1) == 1) ? 'N' : 'n';
                } else if (c >= 0x0149 && c <= 0x014b) {
                    c = ((c & 1) == 0) ? 'N' : 'n';
                } else if (c >= 0x014c && c <= 0x0151) {
                    c = ((c & 1) == 0) ? 'O' : 'o';
                } else if (c >= 0x0154 && c <= 0x0159) {
                    c = ((c & 1) == 0) ? 'R' : 'r';
                } else if (c >= 0x015a && c <= 0x0161) {
                    c = ((c & 1) == 0) ? 'S' : 's';
                } else if (c >= 0x0162 && c <= 0x0167) {
                    c = ((c & 1) == 0) ? 'T' : 't';
                } else if (c >= 0x0168 && c <= 0x0173) {
                    c = ((c & 1) == 0) ? 'U' : 'u';
                } else if (c == 0x0174) {
                    c = 'W';
                } else if (c == 0x0175) {
                    c = 'w';
                } else if (c == 0x0176) {
                    c = 'Y';
                } else if (c == 0x0177) {
                    c = 'y';
                } else if (c == 0x0178) {
                    c = 'Y';
                } else if (c >= 0x0179 && c <= 0x017e) {
                    c = ((c & 1) == 1) ? 'Z' : 'z';
                } else if (c == 0x017f) {
                    c = 's';
                }
            }
            ary[wi++] = c;
        } else if (high == 0x03) {
            // greek
            if (lowmode) {
                if (c >= 0x0391 && c <= 0x03a9) {
                    c += 0x20;
                } else if (c >= 0x03d8 && c <= 0x03ef) {
                    if ((c & 1) == 0) c++;
                } else if (c == 0x0374 || c == 0x03f7 || c == 0x03fa) {
                    c++;
                }
            }
            ary[wi++] = c;
        } else if (high == 0x04) {
            // cyrillic
            if (lowmode) {
                if (c <= 0x040f) {
                    c += 0x50;
                } else if (c <= 0x042f) {
                    c += 0x20;
                } else if (c >= 0x0460 && c <= 0x0481) {
                    if ((c & 1) == 0) c++;
                } else if (c >= 0x048a && c <= 0x04bf) {
                    if ((c & 1) == 0) c++;
                } else if (c == 0x04c0) {
                    c = 0x04cf;
                } else if (c >= 0x04c1 && c <= 0x04ce) {
                    if ((c & 1) == 1) c++;
                } else if (c >= 0x04d0) {
                    if ((c & 1) == 0) c++;
                }
            }
            ary[wi++] = c;
        } else if (high == 0x20) {
            if (c == 0x2002 || c == 0x2003 || c == 0x2009) {
                // en space, em space, thin space
                if (spcmode) {
                    ary[wi++] = 0x0020;
                    if (wi < 2 || ary[wi - 2] == 0x0020) wi--;
                } else {
                    ary[wi++] = c;
                }
            } else if (c == 0x2010) {
                // hyphen
                ary[wi++] = widmode ? 0x002d : c;
            } else if (c == 0x2015) {
                // fullwidth horizontal line
                ary[wi++] = widmode ? 0x002d : c;
            } else if (c == 0x2019) {
                // apostrophe
                ary[wi++] = widmode ? 0x0027 : c;
            } else if (c == 0x2033) {
                // double quotes
                ary[wi++] = widmode ? 0x0022 : c;
            } else {
                // (otherwise)
                ary[wi++] = c;
            }
        } else if (high == 0x22) {
            if (c == 0x2212) {
                // minus sign
                ary[wi++] = widmode ? 0x002d : c;
            } else {
                // (otherwise)
                ary[wi++] = c;
            }
        } else if (high == 0x30) {
            if (c == 0x3000) {
                // fullwidth space
                if (spcmode) {
                    ary[wi++] = 0x0020;
                    if (wi < 2 || ary[wi - 2] == 0x0020) wi--;
                } else if (widmode) {
                    ary[wi++] = 0x0020;
                } else {
                    ary[wi++] = c;
                }
            } else {
                // (otherwise)
                ary[wi++] = c;
            }
        } else if (high == 0xff) {
            if (c == 0xff01) {
                // fullwidth exclamation
                ary[wi++] = widmode ? 0x0021 : c;
            } else if (c == 0xff03) {
                // fullwidth igeta
                ary[wi++] = widmode ? 0x0023 : c;
            } else if (c == 0xff04) {
                // fullwidth dollar
                ary[wi++] = widmode ? 0x0024 : c;
            } else if (c == 0xff05) {
                // fullwidth parcent
                ary[wi++] = widmode ? 0x0025 : c;
            } else if (c == 0xff06) {
                // fullwidth ampersand
                ary[wi++] = widmode ? 0x0026 : c;
            } else if (c == 0xff0a) {
                // fullwidth asterisk
                ary[wi++] = widmode ? 0x002a : c;
            } else if (c == 0xff0b) {
                // fullwidth plus
                ary[wi++] = widmode ? 0x002b : c;
            } else if (c == 0xff0c) {
                // fullwidth comma
                ary[wi++] = widmode ? 0x002c : c;
            } else if (c == 0xff0e) {
                // fullwidth period
                ary[wi++] = widmode ? 0x002e : c;
            } else if (c == 0xff0f) {
                // fullwidth slash
                ary[wi++] = widmode ? 0x002f : c;
            } else if (c == 0xff1a) {
                // fullwidth colon
                ary[wi++] = widmode ? 0x003a : c;
            } else if (c == 0xff1b) {
                // fullwidth semicolon
                ary[wi++] = widmode ? 0x003b : c;
            } else if (c == 0xff1d) {
                // fullwidth equal
                ary[wi++] = widmode ? 0x003d : c;
            } else if (c == 0xff1f) {
                // fullwidth question
                ary[wi++] = widmode ? 0x003f : c;
            } else if (c == 0xff20) {
                // fullwidth atmark
                ary[wi++] = widmode ? 0x0040 : c;
            } else if (c == 0xff3c) {
                // fullwidth backslash
                ary[wi++] = widmode ? 0x005c : c;
            } else if (c == 0xff3e) {
                // fullwidth circumflex
                ary[wi++] = widmode ? 0x005e : c;
            } else if (c == 0xff3f) {
                // fullwidth underscore
                ary[wi++] = widmode ? 0x005f : c;
            } else if (c == 0xff5c) {
                // fullwidth vertical line
                ary[wi++] = widmode ? 0x007c : c;
            } else if (c >= 0xff21 && c <= 0xff3a) {
                // fullwidth alphabets
                if (widmode) {
                    if (lowmode) {
                        ary[wi++] = c - 0xfee0 + 0x20;
                    } else {
                        ary[wi++] = c - 0xfee0;
                    }
                } else if (lowmode) {
                    ary[wi++] = c + 0x20;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff41 && c <= 0xff5a) {
                // fullwidth small alphabets
                ary[wi++] = widmode ? c - 0xfee0 : c;
            } else if (c >= 0xff10 && c <= 0xff19) {
                // fullwidth numbers
                ary[wi++] = widmode ? c - 0xfee0 : c;
            } else if (c == 0xff61) {
                // halfwidth full stop
                ary[wi++] = widmode ? 0x3002 : c;
            } else if (c == 0xff62) {
                // halfwidth left corner
                ary[wi++] = widmode ? 0x300c : c;
            } else if (c == 0xff63) {
                // halfwidth right corner
                ary[wi++] = widmode ? 0x300d : c;
            } else if (c == 0xff64) {
                // halfwidth comma
                ary[wi++] = widmode ? 0x3001 : c;
            } else if (c == 0xff65) {
                // halfwidth middle dot
                ary[wi++] = widmode ? 0x30fb : c;
            } else if (c == 0xff66) {
                // halfwidth wo
                ary[wi++] = widmode ? 0x30f2 : c;
            } else if (c >= 0xff67 && c <= 0xff6b) {
                // halfwidth small a-o
                ary[wi++] = widmode ? (c - 0xff67) * 2 + 0x30a1 : c;
            } else if (c >= 0xff6c && c <= 0xff6e) {
                // halfwidth small ya-yo
                ary[wi++] = widmode ? (c - 0xff6c) * 2 + 0x30e3 : c;
            } else if (c == 0xff6f) {
                // halfwidth small tu
                ary[wi++] = widmode ? 0x30c3 : c;
            } else if (c == 0xff70) {
                // halfwidth prolonged mark
                ary[wi++] = widmode ? 0x30fc : c;
            } else if (c >= 0xff71 && c <= 0xff75) {
                // halfwidth a-o
                if (widmode) {
                    ary[wi] = (c - 0xff71) * 2 + 0x30a2;
                    if (c == 0xff73 && i < num - 1 && ary[i + 1] == 0xff9e) {
                        ary[wi] = 0x30f4;
                        i++;
                    }
                    wi++;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff76 && c <= 0xff7a) {
                // halfwidth ka-ko
                if (widmode) {
                    ary[wi] = (c - 0xff76) * 2 + 0x30ab;
                    if (i < num - 1 && ary[i + 1] == 0xff9e) {
                        ary[wi]++;
                        i++;
                    }
                    wi++;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff7b && c <= 0xff7f) {
                // halfwidth sa-so
                if (widmode) {
                    ary[wi] = (c - 0xff7b) * 2 + 0x30b5;
                    if (i < num - 1 && ary[i + 1] == 0xff9e) {
                        ary[wi]++;
                        i++;
                    }
                    wi++;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff80 && c <= 0xff84) {
                // halfwidth ta-to
                if (widmode) {
                    ary[wi] = (c - 0xff80) * 2 + 0x30bf + (c >= 0xff82 ? 1 : 0);
                    if (i < num - 1 && ary[i + 1] == 0xff9e) {
                        ary[wi]++;
                        i++;
                    }
                    wi++;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff85 && c <= 0xff89) {
                // halfwidth na-no
                ary[wi++] = widmode ? c - 0xcebb : c;
            } else if (c >= 0xff8a && c <= 0xff8e) {
                // halfwidth ha-ho
                if (widmode) {
                    ary[wi] = (c - 0xff8a) * 3 + 0x30cf;
                    if (i < num - 1 && ary[i + 1] == 0xff9e) {
                        ary[wi]++;
                        i++;
                    } else if (i < num - 1 && ary[i + 1] == 0xff9f) {
                        ary[wi] += 2;
                        i++;
                    }
                    wi++;
                } else {
                    ary[wi++] = c;
                }
            } else if (c >= 0xff8f && c <= 0xff93) {
                // halfwidth ma-mo
                ary[wi++] = widmode ? c - 0xceb1 : c;
            } else if (c >= 0xff94 && c <= 0xff96) {
                // halfwidth ya-yo
                ary[wi++] = widmode ? (c - 0xff94) * 2 + 0x30e4 : c;
            } else if (c >= 0xff97 && c <= 0xff9b) {
                // halfwidth ra-ro
                ary[wi++] = widmode ? c - 0xceae : c;
            } else if (c == 0xff9c) {
                // halfwidth wa
                ary[wi++] = widmode ? 0x30ef : c;
            } else if (c == 0xff9d) {
                // halfwidth nn
                ary[wi++] = widmode ? 0x30f3 : c;
            } else {
                // otherwise
                ary[wi++] = c;
            }
        } else {
            // otherwise
            ary[wi++] = c;
        }
    }
    if (spcmode) {
        while (wi > 0 && ary[wi - 1] == 0x0020) {
            wi--;
        }
    }
    return wi;
}

/* Generate a keyword-in-context string from a text and keywords. */
TCLIST *tcstrkwic(const char *str, const TCLIST *words, int width, int opts) {
    assert(str && words && width >= 0);
    TCLIST *texts = tclistnew();
    int len = strlen(str);
    uint16_t *oary, *nary;
    TCMALLOC(oary, sizeof (*oary) * len + 1);
    TCMALLOC(nary, sizeof (*nary) * len + 1);
    int oanum, nanum;
    tcstrutftoucs(str, oary, &oanum);
    tcstrutftoucs(str, nary, &nanum);
    nanum = tcstrucsnorm(nary, nanum, TCUNLOWER | TCUNNOACC | TCUNWIDTH);
    if (nanum != oanum) {
        memcpy(nary, oary, sizeof (*oary) * oanum);
        for (int i = 0; i < oanum; i++) {
            if (nary[i] >= 'A' && nary[i] <= 'Z') nary[i] += 'a' - 'A';
        }
        nanum = oanum;
    }
    int wnum = TCLISTNUM(words);
    TCLIST *uwords = tclistnew2(wnum);
    for (int i = 0; i < wnum; i++) {
        const char *word;
        int wsiz;
        TCLISTVAL(word, words, i, wsiz);
        uint16_t *uwary;
        TCMALLOC(uwary, sizeof (*uwary) * wsiz + 1);
        int uwnum;
        tcstrutftoucs(word, uwary, &uwnum);
        uwnum = tcstrucsnorm(uwary, uwnum, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH);
        if (uwnum > 0) {
            tclistpushmalloc(uwords, uwary, sizeof (*uwary) * uwnum);
        } else {
            TCFREE(uwary);
        }
    }
    wnum = TCLISTNUM(uwords);
    int ri = 0;
    int pi = 0;
    while (ri < nanum) {
        int step = 0;
        for (int i = 0; i < wnum; i++) {
            const char *val;
            int uwnum;
            TCLISTVAL(val, uwords, i, uwnum);
            uint16_t *uwary = (uint16_t *) val;
            uwnum /= sizeof (*uwary);
            if (ri + uwnum <= nanum) {
                int ci = 0;
                while (ci < uwnum && nary[ri + ci] == uwary[ci]) {
                    ci++;
                }
                if (ci == uwnum) {
                    int si = tclmax(ri - width, 0);
                    if (opts & TCKWNOOVER) si = tclmax(si, pi);
                    int ti = tclmin(ri + uwnum + width, nanum);
                    char *tbuf;
                    TCMALLOC(tbuf, (ti - si) * 5 + 1);
                    int wi = 0;
                    if (ri > si) wi += tcstrutfkwicputtext(oary, nary, si, ri, ri,
                            tbuf + wi, uwords, opts);
                    if (opts & TCKWMUTAB) {
                        tbuf[wi++] = '\t';
                    } else if (opts & TCKWMUCTRL) {
                        tbuf[wi++] = 0x02;
                    } else if (opts & TCKWMUBRCT) {
                        tbuf[wi++] = '[';
                    }
                    wi += tcstrucstoutf(oary + ri, ci, tbuf + wi);
                    if (opts & TCKWMUTAB) {
                        tbuf[wi++] = '\t';
                    } else if (opts & TCKWMUCTRL) {
                        tbuf[wi++] = 0x03;
                    } else if (opts & TCKWMUBRCT) {
                        tbuf[wi++] = ']';
                    }
                    if (ti > ri + ci) wi += tcstrutfkwicputtext(oary, nary, ri + ci, ti,
                            nanum, tbuf + wi, uwords, opts);
                    if (wi > 0) {
                        tclistpushmalloc(texts, tbuf, wi);
                    } else {
                        TCFREE(tbuf);
                    }
                    if (ti > step) step = ti;
                    if (step > pi) pi = step;
                    if (opts & TCKWNOOVER) break;
                }
            }
        }
        if (ri == 0 && step < 1 && (opts & TCKWPULEAD)) {
            int ti = tclmin(ri + width * 2, nanum);
            if (ti > 0) {
                char *tbuf;
                TCMALLOC(tbuf, ti * 5 + 1);
                int wi = 0;
                wi += tcstrutfkwicputtext(oary, nary, 0, ti, nanum, tbuf + wi, uwords, opts);
                if (!(opts & TCKWNOOVER) && (opts & TCKWMUTAB)) {
                    tbuf[wi++] = '\t';
                    tbuf[wi++] = '\t';
                }
                tclistpushmalloc(texts, tbuf, wi);
            }
            step = ti;
        }
        if (opts & TCKWNOOVER) {
            ri = (step > 0) ? step : ri + 1;
        } else {
            ri++;
        }
    }
    tclistdel(uwords);
    TCFREE(nary);
    TCFREE(oary);
    return texts;
}

/* Tokenize a text separating by white space characters. */
TCLIST *tcstrtokenize(const char *str) {
    assert(str);
    TCLIST *tokens = tclistnew();
    const unsigned char *rp = (unsigned char *) str;
    while (*rp != '\0') {
        while (*rp <= ' ') {
            rp++;
        }
        if (*rp == '"') {
            rp++;
            TCXSTR *buf = tcxstrnew();
            while (*rp != '\0') {
                if (*rp == '\\') {
                    rp++;
                    if (*rp != '\0') TCXSTRCAT(buf, rp, 1);
                    rp++;
                } else if (*rp == '"') {
                    rp++;
                    break;
                } else {
                    TCXSTRCAT(buf, rp, 1);
                    rp++;
                }
            }
            int size = TCXSTRSIZE(buf);
            tclistpushmalloc(tokens, tcxstrtomalloc(buf), size);
        } else {
            const unsigned char *ep = rp;
            while (*ep > ' ') {
                ep++;
            }
            if (ep > rp) TCLISTPUSH(tokens, rp, ep - rp);
            if (*ep != '\0') {
                rp = ep + 1;
            } else {
                break;
            }
        }
    }
    return tokens;
}

/* Create a list object by splitting a region by zero code. */
TCLIST *tcstrsplit2(const void *ptr, int size) {
    assert(ptr && size >= 0);
    TCLIST *list = tclistnew();
    while (size >= 0) {
        const char *rp = ptr;
        const char *ep = (const char *) ptr + size;
        while (rp < ep) {
            if (*rp == '\0') break;
            rp++;
        }
        TCLISTPUSH(list, ptr, rp - (const char *) ptr);
        rp++;
        size -= rp - (const char *) ptr;
        ptr = rp;
    }
    return list;
}

/* Create a map object by splitting a string. */
TCMAP *tcstrsplit3(const char *str, const char *delims) {
    assert(str && delims);
    TCMAP *map = tcmapnew2(TCMAPTINYBNUM);
    const char *kbuf = NULL;
    int ksiz = 0;
    while (true) {
        const char *sp = str;
        while (*str != '\0' && !strchr(delims, *str)) {
            str++;
        }
        if (kbuf) {
            tcmapput(map, kbuf, ksiz, sp, str - sp);
            kbuf = NULL;
        } else {
            kbuf = sp;
            ksiz = str - sp;
        }
        if (*str == '\0') break;
        str++;
    }
    return map;
}

/* Create a map object by splitting a region by zero code. */
TCMAP *tcstrsplit4(const void *ptr, int size) {
    assert(ptr && size >= 0);
    TCMAP *map = tcmapnew2(tclmin(size / 6 + 1, TCMAPDEFBNUM));
    const char *kbuf = NULL;
    int ksiz = 0;
    while (size >= 0) {
        const char *rp = ptr;
        const char *ep = (const char *) ptr + size;
        while (rp < ep) {
            if (*rp == '\0') break;
            rp++;
        }
        if (kbuf) {
            tcmapput(map, kbuf, ksiz, ptr, rp - (const char *) ptr);
            kbuf = NULL;
        } else {
            kbuf = ptr;
            ksiz = rp - (const char *) ptr;
        }
        rp++;
        size -= rp - (const char *) ptr;
        ptr = rp;
    }
    return map;
}

/* Create a region separated by zero code by joining all elements of a list object. */
void *tcstrjoin2(const TCLIST *list, int *sp) {
    assert(list && sp);
    int num = TCLISTNUM(list);
    int size = num + 1;
    for (int i = 0; i < num; i++) {
        size += TCLISTVALSIZ(list, i);
    }
    char *buf;
    TCMALLOC(buf, size);
    char *wp = buf;
    for (int i = 0; i < num; i++) {
        if (i > 0) *(wp++) = '\0';
        int vsiz;
        const char *vbuf = tclistval(list, i, &vsiz);
        memcpy(wp, vbuf, vsiz);
        wp += vsiz;
    }
    *wp = '\0';
    *sp = wp - buf;
    return buf;
}

/* Create a string by joining all records of a map object. */
char *tcstrjoin3(const TCMAP *map, char delim) {
    assert(map);
    int num = (int) TCMAPRNUM(map);
    int size = num * 2 + 1;
    TCMAPREC *cur = map->cur;
    tcmapiterinit((TCMAP *) map);
    const char *kbuf;
    int ksiz;
    while ((kbuf = tcmapiternext((TCMAP *) map, &ksiz)) != NULL) {
        int vsiz;
        tcmapiterval(kbuf, &vsiz);
        size += ksiz + vsiz;
    }
    char *buf;
    TCMALLOC(buf, size);
    char *wp = buf;
    tcmapiterinit((TCMAP *) map);
    bool first = true;
    while ((kbuf = tcmapiternext((TCMAP *) map, &ksiz)) != NULL) {
        if (first) {
            first = false;
        } else {
            *(wp++) = delim;
        }
        memcpy(wp, kbuf, ksiz);
        wp += ksiz;
        int vsiz;
        const char *vbuf = tcmapiterval(kbuf, &vsiz);
        *(wp++) = delim;
        memcpy(wp, vbuf, vsiz);
        wp += vsiz;
    }
    *wp = '\0';
    ((TCMAP *) map)->cur = cur;
    return buf;
}

/* Create a region separated by zero code by joining all records of a map object. */
void *tcstrjoin4(const TCMAP *map, int *sp) {
    assert(map && sp);
    int num = (int) TCMAPRNUM(map);
    int size = num * 2 + 1;
    TCMAPREC *cur = map->cur;
    tcmapiterinit((TCMAP *) map);
    const char *kbuf;
    int ksiz;
    while ((kbuf = tcmapiternext((TCMAP *) map, &ksiz)) != NULL) {
        int vsiz;
        tcmapiterval(kbuf, &vsiz);
        size += ksiz + vsiz;
    }
    char *buf;
    TCMALLOC(buf, size);
    char *wp = buf;
    tcmapiterinit((TCMAP *) map);
    bool first = true;
    while ((kbuf = tcmapiternext((TCMAP *) map, &ksiz)) != NULL) {
        if (first) {
            first = false;
        } else {
            *(wp++) = '\0';
        }
        memcpy(wp, kbuf, ksiz);
        wp += ksiz;
        int vsiz;
        const char *vbuf = tcmapiterval(kbuf, &vsiz);
        *(wp++) = '\0';
        memcpy(wp, vbuf, vsiz);
        wp += vsiz;
    }
    *wp = '\0';
    *sp = wp - buf;
    ((TCMAP *) map)->cur = cur;
    return buf;
}

/* Sort top records of an array. */
void tctopsort(void *base, size_t nmemb, size_t size, size_t top,
        int(*compar)(const void *, const void *)) {
    assert(base && size > 0 && compar);
    if (nmemb < 1) return;
    if (top > nmemb) top = nmemb;
    char *bp = base;
    char *ep = bp + nmemb * size;
    char *rp = bp + size;
    int num = 1;
    char swap[size];
    while (rp < ep) {
        if (num < top) {
            int cidx = num;
            while (cidx > 0) {
                int pidx = (cidx - 1) / 2;
                if (compar(bp + cidx * size, bp + pidx * size) <= 0) break;
                memcpy(swap, bp + cidx * size, size);
                memcpy(bp + cidx * size, bp + pidx * size, size);
                memcpy(bp + pidx * size, swap, size);
                cidx = pidx;
            }
            num++;
        } else if (compar(rp, bp) < 0) {
            memcpy(swap, bp, size);
            memcpy(bp, rp, size);
            memcpy(rp, swap, size);
            int pidx = 0;
            int bot = num / 2;
            while (pidx < bot) {
                int cidx = pidx * 2 + 1;
                if (cidx < num - 1 && compar(bp + cidx * size, bp + (cidx + 1) * size) < 0) cidx++;
                if (compar(bp + pidx * size, bp + cidx * size) > 0) break;
                memcpy(swap, bp + pidx * size, size);
                memcpy(bp + pidx * size, bp + cidx * size, size);
                memcpy(bp + cidx * size, swap, size);
                pidx = cidx;
            }
        }
        rp += size;
    }
    num = top - 1;
    while (num > 0) {
        memcpy(swap, bp, size);
        memcpy(bp, bp + num * size, size);
        memcpy(bp + num * size, swap, size);
        int pidx = 0;
        int bot = num / 2;
        while (pidx < bot) {
            int cidx = pidx * 2 + 1;
            if (cidx < num - 1 && compar(bp + cidx * size, bp + (cidx + 1) * size) < 0) cidx++;
            if (compar(bp + pidx * size, bp + cidx * size) > 0) break;
            memcpy(swap, bp + pidx * size, size);
            memcpy(bp + pidx * size, bp + cidx * size, size);
            memcpy(bp + cidx * size, swap, size);
            pidx = cidx;
        }
        num--;
    }
}

/* Suspend execution of the current thread. */
bool tcsleep(double sec) {
    if (!isnormal(sec) || sec <= 0.0) return false;
#ifdef _WIN32
    Sleep(sec * 1000);
#else
    if (sec <= 1.0 / sysconf_SC_CLK_TCK) return sched_yield() == 0;
    double integ, fract;
    fract = modf(sec, &integ);
    struct timespec req, rem;
    req.tv_sec = integ;
    req.tv_nsec = tclmin(fract * 1000.0 * 1000.0 * 1000.0, 999999999);
    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) return false;
        req = rem;
    }
#endif
    return true;
}

/* Get the current system information. */
TCMAP *tcsysinfo(void) {
#if defined(_SYS_LINUX_)
    TCMAP *info = tcmapnew2(TCMAPTINYBNUM);
    struct rusage rbuf;
    memset(&rbuf, 0, sizeof (rbuf));
    if (getrusage(RUSAGE_SELF, &rbuf) == 0) {
        tcmapprintf(info, "utime", "%0.6f",
                rbuf.ru_utime.tv_sec + rbuf.ru_utime.tv_usec / 1000000.0);
        tcmapprintf(info, "stime", "%0.6f",
                rbuf.ru_stime.tv_sec + rbuf.ru_stime.tv_usec / 1000000.0);
    }
    TCLIST *lines = tcreadfilelines("/proc/self/status");
    if (lines) {
        int ln = tclistnum(lines);
        for (int i = 0; i < ln; i++) {
            const char *line = TCLISTVALPTR(lines, i);
            const char *rp = strchr(line, ':');
            if (!rp) continue;
            rp++;
            while (*rp > '\0' && *rp <= ' ') {
                rp++;
            }
            if (tcstrifwm(line, "VmSize:")) {
                int64_t size = tcatoix(rp);
                if (size > 0) tcmapprintf(info, "size", "%" PRId64 "", (int64_t) size);
            } else if (tcstrifwm(line, "VmRSS:")) {
                int64_t size = tcatoix(rp);
                if (size > 0) tcmapprintf(info, "rss", "%" PRId64 "", (int64_t) size);
            }
        }
        tclistdel(lines);
    }
    lines = tcreadfilelines("/proc/meminfo");
    if (lines) {
        int ln = tclistnum(lines);
        for (int i = 0; i < ln; i++) {
            const char *line = TCLISTVALPTR(lines, i);
            const char *rp = strchr(line, ':');
            if (!rp) continue;
            rp++;
            while (*rp > '\0' && *rp <= ' ') {
                rp++;
            }
            if (tcstrifwm(line, "MemTotal:")) {
                int64_t size = tcatoix(rp);
                if (size > 0) tcmapprintf(info, "total", "%" PRId64 "", (int64_t) size);
            } else if (tcstrifwm(line, "MemFree:")) {
                int64_t size = tcatoix(rp);
                if (size > 0) tcmapprintf(info, "free", "%" PRId64 "", (int64_t) size);
            } else if (tcstrifwm(line, "Cached:")) {
                int64_t size = tcatoix(rp);
                if (size > 0) tcmapprintf(info, "cached", "%" PRId64 "", (int64_t) size);
            }
        }
        tclistdel(lines);
    }
    lines = tcreadfilelines("/proc/cpuinfo");
    if (lines) {
        int cnum = 0;
        int ln = tclistnum(lines);
        for (int i = 0; i < ln; i++) {
            const char *line = TCLISTVALPTR(lines, i);
            if (tcstrifwm(line, "processor")) cnum++;
        }
        if (cnum > 0) tcmapprintf(info, "corenum", "%" PRId64 "", (int64_t) cnum);
        tclistdel(lines);
    }
    return info;
#elif defined(_SYS_FREEBSD_) || defined(_SYS_MACOSX_)
    TCMAP *info = tcmapnew2(TCMAPTINYBNUM);
    struct rusage rbuf;
    memset(&rbuf, 0, sizeof (rbuf));
    if (getrusage(RUSAGE_SELF, &rbuf) == 0) {
        tcmapprintf(info, "utime", "%0.6f",
                rbuf.ru_utime.tv_sec + rbuf.ru_utime.tv_usec / 1000000.0);
        tcmapprintf(info, "stime", "%0.6f",
                rbuf.ru_stime.tv_sec + rbuf.ru_stime.tv_usec / 1000000.0);
        long tck = sysconf_SC_CLK_TCK;
        int64_t size = (((double) rbuf.ru_ixrss + rbuf.ru_idrss + rbuf.ru_isrss) / tck) * 1024.0;
        if (size > 0) tcmapprintf(info, "rss", "%" PRId64 "", (int64_t) size);
    }
    return info;
#elif defined(_WIN32)
    TCMAP *info = tcmapnew2(TCMAPTINYBNUM);
    FILETIME creat, exit, kernel, user;
    ULARGE_INTEGER largetime;
    if (GetProcessTimes(GetCurrentProcess(), &creat, &exit, &kernel, &user)) {
        largetime.LowPart = user.dwLowDateTime;
        largetime.HighPart = user.dwHighDateTime;
        tcmapprintf(info, "utime", "%0.6f", largetime.QuadPart / 10000000.0);
        largetime.LowPart = kernel.dwLowDateTime;
        largetime.HighPart = kernel.dwHighDateTime;
        tcmapprintf(info, "stime", "%0.6f", largetime.QuadPart / 10000000.0);
    }
    return info;
#else
    TCMAP *info = tcmapnew2(TCMAPTINYBNUM);
    struct rusage rbuf;
    memset(&rbuf, 0, sizeof (rbuf));
    if (getrusage(RUSAGE_SELF, &rbuf) == 0) {
        tcmapprintf(info, "utime", "%0.6f",
                rbuf.ru_utime.tv_sec + rbuf.ru_utime.tv_usec / 1000000.0);
        tcmapprintf(info, "stime", "%0.6f",
                rbuf.ru_stime.tv_sec + rbuf.ru_stime.tv_usec / 1000000.0);
    }
    return info;
#endif
}

/* Create a consistent hashing object. */
TCCHIDX *tcchidxnew(int range) {
    assert(range > 0);
    TCCHIDX *chidx;
    TCMALLOC(chidx, sizeof (*chidx));
    int nnum = range * TCCHIDXVNNUM;
    TCCHIDXNODE *nodes;
    TCMALLOC(nodes, nnum * sizeof (*nodes));
    unsigned int seed = 725;
    for (int i = 0; i < range; i++) {
        int end = (i + 1) * TCCHIDXVNNUM;
        for (int j = i * TCCHIDXVNNUM; j < end; j++) {
            nodes[j].seq = i;
            nodes[j].hash = (seed = seed * 123456761 + 211);
        }
    }
    qsort(nodes, nnum, sizeof (*nodes), tcchidxcmp);
    chidx->nodes = nodes;
    chidx->nnum = nnum;
    return chidx;
}

/* Delete a consistent hashing object. */
void tcchidxdel(TCCHIDX *chidx) {
    assert(chidx);
    TCFREE(chidx->nodes);
    TCFREE(chidx);
}

/* Get the consistent hashing value of a record. */
int tcchidxhash(TCCHIDX *chidx, const void *ptr, int size) {
    assert(chidx && ptr && size >= 0);
    uint32_t hash = 19771007;
    const char *rp = (char *) ptr + size;
    while (size--) {
        hash = (hash * 31) ^ *(uint8_t *)--rp;
        hash ^= hash << 7;
    }
    TCCHIDXNODE *nodes = chidx->nodes;
    int low = 0;
    int high = chidx->nnum;
    while (low < high) {
        int mid = (low + high) >> 1;
        uint32_t nhash = nodes[mid].hash;
        if (hash < nhash) {
            high = mid;
        } else if (hash > nhash) {
            low = mid + 1;
        } else {
            low = mid;
            break;
        }
    }
    if (low >= chidx->nnum) low = 0;
    return nodes[low].seq & INT_MAX;
}

/* Put a text into a KWIC buffer.
   `oary' specifies the original code array.
   `nary' specifies the normalized code array.
   `si' specifies the start index of the text.
   `ti' specifies the terminal index of the text.
   `end' specifies the end index of the code array.
   `buf' specifies the pointer to the output buffer.
   `uwords' specifies the list object of the words to be marked up.
   `opts' specifies the options.
   The return value is the length of the output. */
static int tcstrutfkwicputtext(const uint16_t *oary, const uint16_t *nary, int si, int ti,
        int end, char *buf, const TCLIST *uwords, int opts) {
    assert(oary && nary && si >= 0 && ti >= 0 && end >= 0 && buf && uwords);
    if (!(opts & TCKWNOOVER)) return tcstrucstoutf(oary + si, ti - si, buf);
    if (!(opts & TCKWMUTAB) && !(opts & TCKWMUCTRL) && !(opts & TCKWMUBRCT))
        return tcstrucstoutf(oary + si, ti - si, buf);
    int wnum = TCLISTNUM(uwords);
    int ri = si;
    int wi = 0;
    while (ri < ti) {
        int step = 0;
        for (int i = 0; i < wnum; i++) {
            const char *val;
            int uwnum;
            TCLISTVAL(val, uwords, i, uwnum);
            uint16_t *uwary = (uint16_t *) val;
            uwnum /= sizeof (*uwary);
            if (ri + uwnum <= end) {
                int ci = 0;
                while (ci < uwnum && nary[ri + ci] == uwary[ci]) {
                    ci++;
                }
                if (ci == uwnum) {
                    if (opts & TCKWMUTAB) {
                        buf[wi++] = '\t';
                    } else if (opts & TCKWMUCTRL) {
                        buf[wi++] = 0x02;
                    } else if (opts & TCKWMUBRCT) {
                        buf[wi++] = '[';
                    }
                    wi += tcstrucstoutf(oary + ri, ci, buf + wi);
                    if (opts & TCKWMUTAB) {
                        buf[wi++] = '\t';
                    } else if (opts & TCKWMUCTRL) {
                        buf[wi++] = 0x03;
                    } else if (opts & TCKWMUBRCT) {
                        buf[wi++] = ']';
                    }
                    step = ri + ci;
                    break;
                }
            }
        }
        if (step > 0) {
            ri = step;
        } else {
            wi += tcstrucstoutf(oary + ri, 1, buf + wi);
            ri++;
        }
    }
    return wi;
}

/* Compare two consistent hashing nodes.
   `a' specifies the pointer to one node object.
   `b' specifies the pointer to the other node object.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tcchidxcmp(const void *a, const void *b) {
    if (((TCCHIDXNODE *) a)->hash == ((TCCHIDXNODE *) b)->hash) return 0;
    return ((TCCHIDXNODE *) a)->hash > ((TCCHIDXNODE *) b)->hash;
}



/*************************************************************************************************
 * filesystem utilities
 *************************************************************************************************/


/* Get the canonicalized absolute path of a file. */
#ifndef _WIN32

char *tcrealpath(const char *path) {
    assert(path);
    char buf[PATH_MAX + 1];
    if (realpath(path, buf)) return tcstrdup(buf);
    if (errno == ENOENT) {
        const char *pv = strrchr(path, MYPATHCHR);
        if (pv) {
            if (pv == path) return tcstrdup(path);
            char *prefix = tcmemdup(path, pv - path);
            if (!realpath(prefix, buf)) {
                TCFREE(prefix);
                return NULL;
            }
            TCFREE(prefix);
            pv++;
        } else {
            if (!realpath(MYCDIRSTR, buf)) return NULL;
            pv = path;
        }
        if (buf[0] == MYPATHCHR && buf[1] == '\0') buf[0] = '\0';
        char *str;
        TCMALLOC(str, strlen(buf) + strlen(pv) + 2);
        sprintf(str, "%s%c%s", buf, MYPATHCHR, pv);
        return str;
    }
    return NULL;
}
#else

char *tcrealpath(const char *path) {
    char *ret = _fullpath(NULL, path, _MAX_PATH);
    if (!ret) {
        SetLastError(ERROR_INVALID_ACCESS);
        return NULL;
    }
    return ret;
}
#endif

/* Get the status information of a file. */
bool tcstatfile(const char *path, bool *isdirp, int64_t *sizep, int64_t *mtimep) {
    assert(path);
    struct stat sbuf;
    if (stat(path, &sbuf) != 0) return false;
    if (isdirp) *isdirp = S_ISDIR(sbuf.st_mode);
    if (sizep) *sizep = sbuf.st_size;
    if (mtimep) *mtimep = sbuf.st_mtime;
    return true;
}

/**
 * Unlink (delete) the specified file
 * @param path Path of file
 * @return true of success
 */
bool tcunlinkfile(const char *path) {
#ifndef _WIN32
    return (unlink(path) == 0);
#else
    for (double wsec = 1.0 / sysconf_SC_CLK_TCK; true; wsec *= 2) {
        if (unlink(path)) {
            if (errno != EACCES) {
                break;
            }
        } else {
            return true;
        }
        if (wsec > 8.0) {
            break;
        }
        tcsleep(wsec);
    }
    return false;
#endif
}

/**
 * Rename the specified file
 */
bool tcrenamefile(const char *from, const char* to) {
#ifndef _WIN32
    return (rename(from, to) == 0);
#else
    for (double wsec = 1.0 / sysconf_SC_CLK_TCK; true; wsec *= 2) {
        if (rename(from, to)) {
            if (errno != EACCES) {
                break;
            }
        } else {
            return true;
        }
        if (wsec > 8.0) {
            break;
        }
        tcsleep(wsec);
    }
    return false;
#endif
}

/* Read whole data of a file. */
void *tcreadfile(const char *path, int limit, int *sp) {
    HANDLE fd;
    if (path) {
#ifndef _WIN32
        fd = open(path, O_RDONLY, TCFILEMODE);
#else
        fd = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    } else {
        fd = GET_STDIN_HANDLE();
    }
    if (INVALIDHANDLE(fd)) {
        return NULL;
    }
    if (path == NULL) { //stdin
        TCXSTR *xstr = tcxstrnew();
        char buf[TCIOBUFSIZ];
        limit = limit > 0 ? limit : INT_MAX;
        int rsiz;

        while (1) {
#ifndef _WIN32
            rsiz = read(fd, buf, tclmin(TCIOBUFSIZ, limit));
#else
            DWORD red;
            if (!ReadFile(fd, buf, tclmin(TCIOBUFSIZ, limit), &red, NULL)) {
                rsiz = -1;
            } else {
                rsiz = red;
            }
#endif
            if (rsiz <= 0) {
                break;
            }
            TCXSTRCAT(xstr, buf, rsiz);
            limit -= rsiz;
        }
        if (sp) *sp = TCXSTRSIZE(xstr);
        return tcxstrtomalloc(xstr);
    }
    struct stat sbuf;
    if (fstat(fd, &sbuf) || !S_ISREG(sbuf.st_mode)) {
        CLOSEFH(fd);
        return NULL;
    }
    limit = (limit > 0) ? tclmin((int) sbuf.st_size, limit) : sbuf.st_size;
    char *buf;
    TCMALLOC(buf, limit + 1);
    char *wp = buf;
    int rsiz;
    while (1) {
#ifndef _WIN32
        rsiz = read(fd, wp, limit - (wp - buf));
#else
        DWORD red;
        if (!ReadFile(fd, wp, limit - (wp - buf), &red, NULL)) {
            rsiz = -1;
        } else {
            rsiz = red;
        }
#endif
        if (rsiz <= 0) {
            break;
        }
        wp += rsiz;
    }
    *wp = '\0';
    CLOSEFH(fd);
    if (sp) *sp = wp - buf;
    return buf;
}

/* Read every line of a file. */
TCLIST *tcreadfilelines(const char *path) {
    int fd = path ? open(path, O_RDONLY, TCFILEMODE) : 0;
    if (fd == -1) return NULL;
    TCLIST *list = tclistnew();
    TCXSTR *xstr = tcxstrnew();
    char buf[TCIOBUFSIZ];
    int rsiz;
    while ((rsiz = read(fd, buf, TCIOBUFSIZ)) > 0) {
        for (int i = 0; i < rsiz; i++) {
            switch (buf[i]) {
                case '\r':
                    break;
                case '\n':
                    TCLISTPUSH(list, TCXSTRPTR(xstr), TCXSTRSIZE(xstr));
                    tcxstrclear(xstr);
                    break;
                default:
                    TCXSTRCAT(xstr, buf + i, 1);
                    break;
            }
        }
    }
    TCLISTPUSH(list, TCXSTRPTR(xstr), TCXSTRSIZE(xstr));
    tcxstrdel(xstr);
    if (path) close(fd);
    return list;
}

/* Write data into a file. */
bool tcwritefile(const char *path, const void *ptr, int size) {
    assert(ptr && size >= 0);
    HANDLE fd;
    if (path) {
#ifdef _WIN32
        fd = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, TCFILEMODE);
#endif
    } else {
        fd = GET_STDOUT_HANDLE();
    }
    if (INVALIDHANDLE(fd)) {
        return false;
    }
    bool err = false;
    if (!tcwrite(fd, ptr, size)) err = true;
    if (path && !CLOSEFH(fd)) err = true;
    return !err;
}

/* Copy a file. */
bool tccopyfile(const char *src, const char *dest) {
    HANDLE ifd, ofd;
#ifndef _WIN32
    ifd = open(src, O_RDONLY, TCFILEMODE);
#else
    ifd = CreateFile(src, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    if (INVALIDHANDLE(ifd)) return false;

#ifndef _WIN32
    ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, TCFILEMODE);
#else
    ofd = CreateFile(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    if (INVALIDHANDLE(ofd)) {
        CLOSEFH(ifd);
        return false;
    }
    bool err = false;
    struct stat stat;
    if (fstat(ifd, &stat)) {
        CLOSEFH(ifd);
        CLOSEFH(ofd);
        return false;
    }
    off_t togo = stat.st_size;
#ifdef _WIN32
    HANDLE map = CreateFileMapping(ifd, NULL, PAGE_READONLY, 0, 0, NULL);
    void *mmaped;
    if (INVALIDHANDLE(map)) {
        CLOSEFH(ifd);
        CLOSEFH(ofd);
        return false;
    }
#endif
    while (togo > 0) {
        char buf[TCIOBUFSIZ];
#ifndef _WIN32
        int size = read(ifd, buf, TCIOBUFSIZ);
#else
        size_t toread = min(togo, TCIOBUFSIZ);
        int size;
        LARGE_INTEGER offset;
        offset.QuadPart = stat.st_size - togo;
        mmaped = MapViewOfFileEx(map, FILE_MAP_READ, offset.HighPart, offset.LowPart, toread, NULL);
        if (mmaped == NULL) {
            size = -1;
            errno = ESPIPE;
        } else {
            size = toread;
            memcpy(buf, mmaped, toread);
            UnmapViewOfFile(mmaped);
        }
#endif
        if (size > 0) {
            togo -= size;
            if (!tcwrite(ofd, buf, size)) {
                err = true;
                break;
            }
        } else if (size == -1) {
            if (errno != EINTR) {
                err = true;
                break;
            }
        } else {
            break;
        }
    }
#ifdef _WIN32
    if (!CLOSEFH(map)) err = true;
#endif
    if (!CLOSEFH(ofd)) err = true;
    if (!CLOSEFH(ifd)) err = true;
    return !err;
}

/* Read names of files in a directory. */
TCLIST *tcreaddir(const char *path) {
    assert(path);
    DIR *DD;
    struct dirent *dp;
    if (!(DD = opendir(path))) return NULL;
    TCLIST *list = tclistnew();
    while ((dp = readdir(DD)) != NULL) {
        if (!strcmp(dp->d_name, MYCDIRSTR) || !strcmp(dp->d_name, MYPDIRSTR)) continue;
        TCLISTPUSH(list, dp->d_name, strlen(dp->d_name));
    }
    closedir(DD);
    return list;
}

/* Expand a pattern into a list of matched paths. */
TCLIST *tcglobpat(const char *pattern) {
    assert(pattern);
    TCLIST *list = tclistnew();
    glob_t gbuf;
    memset(&gbuf, 0, sizeof (gbuf));
    if (glob(pattern, GLOB_ERR | GLOB_NOSORT, NULL, &gbuf) == 0) {
        for (int i = 0; i < gbuf.gl_pathc; i++) {
            tclistpush2(list, gbuf.gl_pathv[i]);
        }
        globfree(&gbuf);
    }
    return list;
}

/* Remove a file or a directory and its sub ones recursively. */
bool tcremovelink(const char *path) {
    assert(path);
    struct stat sbuf;
    if (lstat(path, &sbuf)) return false;
    if (unlink(path) == 0) return true;
    TCLIST *list;
    if (!S_ISDIR(sbuf.st_mode) || !(list = tcreaddir(path))) return false;
    bool tail = path[0] != '\0' && path[strlen(path) - 1] == MYPATHCHR;
    for (int i = 0; i < TCLISTNUM(list); i++) {
        const char *elem = TCLISTVALPTR(list, i);
        if (!strcmp(MYCDIRSTR, elem) || !strcmp(MYPDIRSTR, elem)) continue;
        char *cpath;
        if (tail) {
            cpath = tcsprintf("%s%s", path, elem);
        } else {
            cpath = tcsprintf("%s%c%s", path, MYPATHCHR, elem);
        }
        tcremovelink(cpath);
        TCFREE(cpath);
    }
    tclistdel(list);
    return rmdir(path) == 0 ? true : false;
}

bool tcfseek(HANDLE fd, off_t off, int whence) {
#ifdef _WIN32
    LARGE_INTEGER loff;
    loff.QuadPart = off;
    int w = FILE_BEGIN;
    if (whence == TCFCUR) {
        w = FILE_CURRENT;
    } else if (whence == TCFEND) {
        w = FILE_END;
    }
    return SetFilePointerEx(fd, loff, NULL, w);
#else
    int w = SEEK_SET;
    if (whence == TCFCUR) {
        w = SEEK_CUR;
    } else if (whence == TCFEND) {
        w = SEEK_END;
    }
    return (lseek(fd, off, w) != -1);
#endif
}

bool tcftruncate(HANDLE fd, off_t length) {
#ifdef _WIN32
    LARGE_INTEGER size;
    size.QuadPart = length;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    bool err = false;
    pthread_mutex_lock(&mutex);
    if (!SetFilePointerEx(fd, size, NULL, FILE_BEGIN)) err = true;
    if (err == false && !SetEndOfFile(fd)) err = true;
    pthread_mutex_unlock(&mutex);
    return !err;
#else
    return (ftruncate(fd, length) != -1);
#endif
}

/* Write data into a file. */
bool tcwrite(HANDLE fd, const void *buf, size_t size) {
    assert(!INVALIDHANDLE(fd) && buf && size >= 0);
    const char *rp = buf;
    do {
#ifndef _WIN32
        int wb = write(fd, rp, size);
#else
        DWORD written;
        int wb;
        if (!WriteFile(fd, rp, size, &written, NULL))
            wb = -1;
        else
            wb = written;
#endif
        switch (wb) {
            case -1: if (errno != EINTR) return false;
            case 0: break;
            default:
                rp += wb;
                size -= wb;
                break;
        }
    } while (size > 0);
    return true;
}

/* Read data from a file. */
bool tcread(HANDLE fd, void *buf, size_t size) {
    assert(!INVALIDHANDLE(fd) && buf && size >= 0);
    char *wp = buf;
    do {
#ifndef _WIN32
        int rb = read(fd, wp, size);
#else
        int rb;
        DWORD r;
        if (!ReadFile(fd, wp, size, &r, NULL))
            rb = -1;
        else
            rb = r;
#endif
        switch (rb) {
            case -1: if (errno != EINTR) return false;
            case 0: return size < 1;
            default:
                wp += rb;
                size -= rb;
        }
    } while (size > 0);
    return true;
}

/* Lock a file. */
bool tclock(HANDLE fd, bool ex, bool nb) {
    assert(!INVALIDHANDLE(fd));
#ifndef _WIN32
    struct flock lock;
    memset(&lock, 0, sizeof (struct flock));
    lock.l_type = ex ? F_WRLCK : F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = 0;
    while (fcntl(fd, nb ? F_SETLK : F_SETLKW, &lock) == -1) {
        if (errno != EINTR) return false;
    }
    return true;
#else
    DWORD type = 0; /* shared lock with waiting */
    OVERLAPPED offset;
    memset(&offset, 0, sizeof (OVERLAPPED));
    if (ex) type = LOCKFILE_EXCLUSIVE_LOCK;
    if (nb) type |= LOCKFILE_FAIL_IMMEDIATELY;
    if (LockFileEx(fd, type, 0, ULONG_MAX, ULONG_MAX, &offset)) {
        return true;
    } else {
        return false;
    }
#endif
}

/* Unlock a file. */
bool tcunlock(HANDLE fd) {
    assert(!INVALIDHANDLE(fd));
#ifndef _WIN32
    struct flock lock;
    memset(&lock, 0, sizeof (struct flock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = 0;
    while (fcntl(fd, F_SETLKW, &lock) == -1) {
        if (errno != EINTR) return false;
    }
    return true;
#else
    OVERLAPPED offset;
    memset(&offset, 0, sizeof (OVERLAPPED));
    if (UnlockFileEx(fd, 0, ULONG_MAX, ULONG_MAX, &offset)) {
        return true;
    } else {
        return false;
    }
#endif
}

/* Execute a shell command. */
int tcsystem(const char **args, int anum) {
    assert(args && anum >= 0);
    if (anum < 1) return -1;
    TCXSTR *phrase = tcxstrnew3(anum * TCNUMBUFSIZ + 1);
    for (int i = 0; i < anum; i++) {
        const char *rp = args[i];
        int len = strlen(rp);
        char *token;
        TCMALLOC(token, len * 2 + 1);
        char *wp = token;
        while (*rp != '\0') {
            switch (*rp) {
                case '"': case '\\': case '$': case '`':
                    *(wp++) = '\\';
                    *(wp++) = *rp;
                    break;
                default:
                    *(wp++) = *rp;
                    break;
            }
            rp++;
        }
        *wp = '\0';
        if (tcxstrsize(phrase)) tcxstrcat(phrase, " ", 1);
        tcxstrprintf(phrase, "\"%s\"", token);
        TCFREE(token);
    }
    int rv = system(tcxstrptr(phrase));
    if (WIFEXITED(rv)) {
        rv = WEXITSTATUS(rv);
    } else {
        rv = INT_MAX;
    }
    tcxstrdel(phrase);
    return rv;
}



/*************************************************************************************************
 * encoding utilities
 *************************************************************************************************/


#define TCENCBUFSIZ    32                // size of a buffer for encoding name
#define TCXMLATBNUM    31                // bucket number of XML attributes

/* Encode a serial object with URL encoding. */
char *tcurlencode(const char *ptr, int size) {
    assert(ptr && size >= 0);
    char *buf;
    TCMALLOC(buf, size * 3 + 1);
    char *wp = buf;
    for (int i = 0; i < size; i++) {
        int c = ((unsigned char *) ptr)[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || (c != '\0' && strchr("_-.!~*'()", c))) {
            *(wp++) = c;
        } else {
            wp += sprintf(wp, "%%%02X", c);
        }
    }
    *wp = '\0';
    return buf;
}

/* Decode a string encoded with URL encoding. */
char *tcurldecode(const char *str, int *sp) {
    assert(str && sp);
    char *buf = tcstrdup(str);
    char *wp = buf;
    while (*str != '\0') {
        if (*str == '%') {
            str++;
            if (((str[0] >= '0' && str[0] <= '9') || (str[0] >= 'A' && str[0] <= 'F') ||
                    (str[0] >= 'a' && str[0] <= 'f')) &&
                    ((str[1] >= '0' && str[1] <= '9') || (str[1] >= 'A' && str[1] <= 'F') ||
                    (str[1] >= 'a' && str[1] <= 'f'))) {
                unsigned char c = *str;
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (c >= 'a' && c <= 'z') {
                    *wp = c - 'a' + 10;
                } else {
                    *wp = c - '0';
                }
                *wp *= 0x10;
                str++;
                c = *str;
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (c >= 'a' && c <= 'z') {
                    *wp += c - 'a' + 10;
                } else {
                    *wp += c - '0';
                }
                str++;
                wp++;
            } else {
                break;
            }
        } else if (*str == '+') {
            *wp = ' ';
            str++;
            wp++;
        } else {
            *wp = *str;
            str++;
            wp++;
        }
    }
    *wp = '\0';
    *sp = wp - buf;
    return buf;
}

/* Break up a URL into elements. */
TCMAP *tcurlbreak(const char *str) {
    assert(str);
    TCMAP *map = tcmapnew2(TCMAPTINYBNUM);
    char *trim = tcstrdup(str);
    tcstrtrim(trim);
    const char *rp = trim;
    char *norm;
    TCMALLOC(norm, strlen(trim) * 3 + 1);
    char *wp = norm;
    while (*rp != '\0') {
        if (*rp > 0x20 && *rp < 0x7f) {
            *(wp++) = *rp;
        } else {
            wp += sprintf(wp, "%%%02X", *(unsigned char *) rp);
        }
        rp++;
    }
    *wp = '\0';
    rp = norm;
    tcmapput2(map, "self", rp);
    bool serv = false;
    if (tcstrifwm(rp, "http://")) {
        tcmapput2(map, "scheme", "http");
        rp += 7;
        serv = true;
    } else if (tcstrifwm(rp, "https://")) {
        tcmapput2(map, "scheme", "https");
        rp += 8;
        serv = true;
    } else if (tcstrifwm(rp, "ftp://")) {
        tcmapput2(map, "scheme", "ftp");
        rp += 6;
        serv = true;
    } else if (tcstrifwm(rp, "sftp://")) {
        tcmapput2(map, "scheme", "sftp");
        rp += 7;
        serv = true;
    } else if (tcstrifwm(rp, "ftps://")) {
        tcmapput2(map, "scheme", "ftps");
        rp += 7;
        serv = true;
    } else if (tcstrifwm(rp, "tftp://")) {
        tcmapput2(map, "scheme", "tftp");
        rp += 7;
        serv = true;
    } else if (tcstrifwm(rp, "ldap://")) {
        tcmapput2(map, "scheme", "ldap");
        rp += 7;
        serv = true;
    } else if (tcstrifwm(rp, "ldaps://")) {
        tcmapput2(map, "scheme", "ldaps");
        rp += 8;
        serv = true;
    } else if (tcstrifwm(rp, "file://")) {
        tcmapput2(map, "scheme", "file");
        rp += 7;
        serv = true;
    }
    char *ep;
    if ((ep = strchr(rp, '#')) != NULL) {
        tcmapput2(map, "fragment", ep + 1);
        *ep = '\0';
    }
    if ((ep = strchr(rp, '?')) != NULL) {
        tcmapput2(map, "query", ep + 1);
        *ep = '\0';
    }
    if (serv) {
        if ((ep = strchr(rp, '/')) != NULL) {
            tcmapput2(map, "path", ep);
            *ep = '\0';
        } else {
            tcmapput2(map, "path", "/");
        }
        if ((ep = strchr(rp, '@')) != NULL) {
            *ep = '\0';
            if (rp[0] != '\0') tcmapput2(map, "authority", rp);
            rp = ep + 1;
        }
        if ((ep = strchr(rp, ':')) != NULL) {
            if (ep[1] != '\0') tcmapput2(map, "port", ep + 1);
            *ep = '\0';
        }
        if (rp[0] != '\0') tcmapput2(map, "host", rp);
    } else {
        tcmapput2(map, "path", rp);
    }
    TCFREE(norm);
    TCFREE(trim);
    if ((rp = tcmapget2(map, "path")) != NULL) {
        if ((ep = strrchr(rp, '/')) != NULL) {
            if (ep[1] != '\0') tcmapput2(map, "file", ep + 1);
        } else {
            tcmapput2(map, "file", rp);
        }
    }
    if ((rp = tcmapget2(map, "file")) != NULL && (!strcmp(rp, ".") || !strcmp(rp, "..")))
        tcmapout2(map, "file");
    return map;
}

/* Resolve a relative URL with an absolute URL. */
char *tcurlresolve(const char *base, const char *target) {
    assert(base && target);
    const char *vbuf, *path;
    char *tmp, *wp, *enc;
    while (*base > '\0' && *base <= ' ') {
        base++;
    }
    while (*target > '\0' && *target <= ' ') {
        target++;
    }
    if (*target == '\0') target = base;
    TCXSTR *rbuf = tcxstrnew();
    TCMAP *telems = tcurlbreak(target);
    int port = 80;
    TCMAP *belems = tcurlbreak(tcmapget2(telems, "scheme") ? target : base);
    if ((vbuf = tcmapget2(belems, "scheme")) != NULL) {
        tcxstrcat2(rbuf, vbuf);
        TCXSTRCAT(rbuf, "://", 3);
        if (!tcstricmp(vbuf, "https")) {
            port = 443;
        } else if (!tcstricmp(vbuf, "ftp")) {
            port = 21;
        } else if (!tcstricmp(vbuf, "sftp")) {
            port = 115;
        } else if (!tcstricmp(vbuf, "ftps")) {
            port = 22;
        } else if (!tcstricmp(vbuf, "tftp")) {
            port = 69;
        } else if (!tcstricmp(vbuf, "ldap")) {
            port = 389;
        } else if (!tcstricmp(vbuf, "ldaps")) {
            port = 636;
        }
    } else {
        tcxstrcat2(rbuf, "http://");
    }
    int vsiz;
    if ((vbuf = tcmapget2(belems, "authority")) != NULL) {
        if ((wp = strchr(vbuf, ':')) != NULL) {
            *wp = '\0';
            tmp = tcurldecode(vbuf, &vsiz);
            enc = tcurlencode(tmp, vsiz);
            tcxstrcat2(rbuf, enc);
            TCFREE(enc);
            TCFREE(tmp);
            TCXSTRCAT(rbuf, ":", 1);
            wp++;
            tmp = tcurldecode(wp, &vsiz);
            enc = tcurlencode(tmp, vsiz);
            tcxstrcat2(rbuf, enc);
            TCFREE(enc);
            TCFREE(tmp);
        } else {
            tmp = tcurldecode(vbuf, &vsiz);
            enc = tcurlencode(tmp, vsiz);
            tcxstrcat2(rbuf, enc);
            TCFREE(enc);
            TCFREE(tmp);
        }
        TCXSTRCAT(rbuf, "@", 1);
    }
    if ((vbuf = tcmapget2(belems, "host")) != NULL) {
        tmp = tcurldecode(vbuf, &vsiz);
        tcstrtolower(tmp);
        enc = tcurlencode(tmp, vsiz);
        tcxstrcat2(rbuf, enc);
        TCFREE(enc);
        TCFREE(tmp);
    } else {
        TCXSTRCAT(rbuf, "localhost", 9);
    }
    int num;
    char numbuf[TCNUMBUFSIZ];
    if ((vbuf = tcmapget2(belems, "port")) != NULL && (num = tcatoi(vbuf)) != port && num > 0) {
        sprintf(numbuf, ":%d", num);
        tcxstrcat2(rbuf, numbuf);
    }
    if (!(path = tcmapget2(telems, "path"))) path = "/";
    if (path[0] == '\0' && (vbuf = tcmapget2(belems, "path")) != NULL) path = vbuf;
    if (path[0] == '\0') path = "/";
    TCLIST *bpaths = tclistnew();
    TCLIST *opaths;
    if (path[0] != '/' && (vbuf = tcmapget2(belems, "path")) != NULL) {
        opaths = tcstrsplit(vbuf, "/");
    } else {
        opaths = tcstrsplit("/", "/");
    }
    TCFREE(tclistpop2(opaths));
    for (int i = 0; i < TCLISTNUM(opaths); i++) {
        vbuf = tclistval(opaths, i, &vsiz);
        if (vsiz < 1 || !strcmp(vbuf, ".")) continue;
        if (!strcmp(vbuf, "..")) {
            TCFREE(tclistpop2(bpaths));
        } else {
            TCLISTPUSH(bpaths, vbuf, vsiz);
        }
    }
    tclistdel(opaths);
    opaths = tcstrsplit(path, "/");
    for (int i = 0; i < TCLISTNUM(opaths); i++) {
        vbuf = tclistval(opaths, i, &vsiz);
        if (vsiz < 1 || !strcmp(vbuf, ".")) continue;
        if (!strcmp(vbuf, "..")) {
            TCFREE(tclistpop2(bpaths));
        } else {
            TCLISTPUSH(bpaths, vbuf, vsiz);
        }
    }
    tclistdel(opaths);
    for (int i = 0; i < TCLISTNUM(bpaths); i++) {
        vbuf = TCLISTVALPTR(bpaths, i);
        if (strchr(vbuf, '%')) {
            tmp = tcurldecode(vbuf, &vsiz);
        } else {
            tmp = tcstrdup(vbuf);
        }
        enc = tcurlencode(tmp, strlen(tmp));
        TCXSTRCAT(rbuf, "/", 1);
        tcxstrcat2(rbuf, enc);
        TCFREE(enc);
        TCFREE(tmp);
    }
    if (tcstrbwm(path, "/")) TCXSTRCAT(rbuf, "/", 1);
    tclistdel(bpaths);
    if ((vbuf = tcmapget2(telems, "query")) != NULL ||
            (*target == '#' && (vbuf = tcmapget2(belems, "query")) != NULL)) {
        TCXSTRCAT(rbuf, "?", 1);
        TCLIST *qelems = tcstrsplit(vbuf, "&;");
        for (int i = 0; i < TCLISTNUM(qelems); i++) {
            vbuf = TCLISTVALPTR(qelems, i);
            if (i > 0) TCXSTRCAT(rbuf, "&", 1);
            if ((wp = strchr(vbuf, '=')) != NULL) {
                *wp = '\0';
                tmp = tcurldecode(vbuf, &vsiz);
                enc = tcurlencode(tmp, vsiz);
                tcxstrcat2(rbuf, enc);
                TCFREE(enc);
                TCFREE(tmp);
                TCXSTRCAT(rbuf, "=", 1);
                wp++;
                tmp = tcurldecode(wp, &vsiz);
                enc = tcurlencode(tmp, strlen(tmp));
                tcxstrcat2(rbuf, enc);
                TCFREE(enc);
                TCFREE(tmp);
            } else {
                tmp = tcurldecode(vbuf, &vsiz);
                enc = tcurlencode(tmp, vsiz);
                tcxstrcat2(rbuf, enc);
                TCFREE(enc);
                TCFREE(tmp);
            }
        }
        tclistdel(qelems);
    }
    if ((vbuf = tcmapget2(telems, "fragment")) != NULL) {
        tmp = tcurldecode(vbuf, &vsiz);
        enc = tcurlencode(tmp, vsiz);
        TCXSTRCAT(rbuf, "#", 1);
        tcxstrcat2(rbuf, enc);
        TCFREE(enc);
        TCFREE(tmp);
    }
    tcmapdel(belems);
    tcmapdel(telems);
    return tcxstrtomalloc(rbuf);
}

/* Encode a serial object with Base64 encoding. */
char *tcbaseencode(const char *ptr, int size) {
    assert(ptr && size >= 0);
    char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *obj = (const unsigned char *) ptr;
    char *buf;
    TCMALLOC(buf, 4 * (size + 2) / 3 + 1);
    char *wp = buf;
    for (int i = 0; i < size; i += 3) {
        switch (size - i) {
            case 1:
                *wp++ = tbl[obj[0] >> 2];
                *wp++ = tbl[(obj[0] & 3) << 4];
                *wp++ = '=';
                *wp++ = '=';
                break;
            case 2:
                *wp++ = tbl[obj[0] >> 2];
                *wp++ = tbl[((obj[0] & 3) << 4) + (obj[1] >> 4)];
                *wp++ = tbl[(obj[1] & 0xf) << 2];
                *wp++ = '=';
                break;
            default:
                *wp++ = tbl[obj[0] >> 2];
                *wp++ = tbl[((obj[0] & 3) << 4) + (obj[1] >> 4)];
                *wp++ = tbl[((obj[1] & 0xf) << 2) + (obj[2] >> 6)];
                *wp++ = tbl[obj[2] & 0x3f];
                break;
        }
        obj += 3;
    }
    *wp = '\0';
    return buf;
}

/* Decode a string encoded with Base64 encoding. */
char *tcbasedecode(const char *str, int *sp) {
    assert(str && sp);
    int cnt = 0;
    int bpos = 0;
    int eqcnt = 0;
    int len = strlen(str);
    unsigned char *obj;
    TCMALLOC(obj, len + 4);
    unsigned char *wp = obj;
    while (bpos < len && eqcnt == 0) {
        int bits = 0;
        int i;
        for (i = 0; bpos < len && i < 4; bpos++) {
            if (str[bpos] >= 'A' && str[bpos] <= 'Z') {
                bits = (bits << 6) | (str[bpos] - 'A');
                i++;
            } else if (str[bpos] >= 'a' && str[bpos] <= 'z') {
                bits = (bits << 6) | (str[bpos] - 'a' + 26);
                i++;
            } else if (str[bpos] >= '0' && str[bpos] <= '9') {
                bits = (bits << 6) | (str[bpos] - '0' + 52);
                i++;
            } else if (str[bpos] == '+') {
                bits = (bits << 6) | 62;
                i++;
            } else if (str[bpos] == '/') {
                bits = (bits << 6) | 63;
                i++;
            } else if (str[bpos] == '=') {
                bits <<= 6;
                i++;
                eqcnt++;
            }
        }
        if (i == 0 && bpos >= len) continue;
        switch (eqcnt) {
            case 0:
                *wp++ = (bits >> 16) & 0xff;
                *wp++ = (bits >> 8) & 0xff;
                *wp++ = bits & 0xff;
                cnt += 3;
                break;
            case 1:
                *wp++ = (bits >> 16) & 0xff;
                *wp++ = (bits >> 8) & 0xff;
                cnt += 2;
                break;
            case 2:
                *wp++ = (bits >> 16) & 0xff;
                cnt += 1;
                break;
        }
    }
    obj[cnt] = '\0';
    *sp = cnt;
    return (char *) obj;
}

/* Encode a serial object with Quoted-printable encoding. */
char *tcquoteencode(const char *ptr, int size) {
    assert(ptr && size >= 0);
    const unsigned char *rp = (const unsigned char *) ptr;
    char *buf;
    TCMALLOC(buf, size * 3 + 1);
    char *wp = buf;
    int cols = 0;
    for (int i = 0; i < size; i++) {
        if (rp[i] == '=' || (rp[i] < 0x20 && rp[i] != '\r' && rp[i] != '\n' && rp[i] != '\t') ||
                rp[i] > 0x7e) {
            wp += sprintf(wp, "=%02X", rp[i]);
            cols += 3;
        } else {
            *(wp++) = rp[i];
            cols++;
        }
    }
    *wp = '\0';
    return buf;
}

/* Decode a string encoded with Quoted-printable encoding. */
char *tcquotedecode(const char *str, int *sp) {
    assert(str && sp);
    char *buf;
    TCMALLOC(buf, strlen(str) + 1);
    char *wp = buf;
    for (; *str != '\0'; str++) {
        if (*str == '=') {
            str++;
            if (*str == '\0') {
                break;
            } else if (str[0] == '\r' && str[1] == '\n') {
                str++;
            } else if (str[0] != '\n' && str[0] != '\r') {
                if (*str >= 'A' && *str <= 'Z') {
                    *wp = (*str - 'A' + 10) * 16;
                } else if (*str >= 'a' && *str <= 'z') {
                    *wp = (*str - 'a' + 10) * 16;
                } else {
                    *wp = (*str - '0') * 16;
                }
                str++;
                if (*str == '\0') break;
                if (*str >= 'A' && *str <= 'Z') {
                    *wp += *str - 'A' + 10;
                } else if (*str >= 'a' && *str <= 'z') {
                    *wp += *str - 'a' + 10;
                } else {
                    *wp += *str - '0';
                }
                wp++;
            }
        } else {
            *wp = *str;
            wp++;
        }
    }
    *wp = '\0';
    *sp = wp - buf;
    return buf;
}

/* Encode a string with MIME encoding. */
char *tcmimeencode(const char *str, const char *encname, bool base) {
    assert(str && encname);
    int len = strlen(str);
    char *buf;
    TCMALLOC(buf, len * 3 + strlen(encname) + 16);
    char *wp = buf;
    wp += sprintf(wp, "=?%s?%c?", encname, base ? 'B' : 'Q');
    char *enc = base ? tcbaseencode(str, len) : tcquoteencode(str, len);
    wp += sprintf(wp, "%s?=", enc);
    TCFREE(enc);
    return buf;
}

/* Decode a string encoded with MIME encoding. */
char *tcmimedecode(const char *str, char *enp) {
    assert(str);
    if (enp) sprintf(enp, "US-ASCII");
    char *buf;
    TCMALLOC(buf, strlen(str) + 1);
    char *wp = buf;
    while (*str != '\0') {
        if (tcstrfwm(str, "=?")) {
            str += 2;
            const char *pv = str;
            const char *ep = strchr(str, '?');
            if (!ep) continue;
            if (enp && ep - pv < TCENCBUFSIZ) {
                memcpy(enp, pv, ep - pv);
                enp[ep - pv] = '\0';
            }
            pv = ep + 1;
            bool quoted = (*pv == 'Q' || *pv == 'q');
            if (*pv != '\0') pv++;
            if (*pv != '\0') pv++;
            if (!(ep = strchr(pv, '?'))) continue;
            char *tmp;
            TCMEMDUP(tmp, pv, ep - pv);
            int len;
            char *dec = quoted ? tcquotedecode(tmp, &len) : tcbasedecode(tmp, &len);
            wp += sprintf(wp, "%s", dec);
            TCFREE(dec);
            TCFREE(tmp);
            str = ep + 1;
            if (*str != '\0') str++;
        } else {
            *(wp++) = *str;
            str++;
        }
    }
    *wp = '\0';
    return buf;
}

/* Split a string of MIME into headers and the body. */
char *tcmimebreak(const char *ptr, int size, TCMAP *headers, int *sp) {
    assert(ptr && size >= 0 && sp);
    const char *head = NULL;
    int hlen = 0;
    for (int i = 0; i < size; i++) {
        if (i < size - 4 && ptr[i] == '\r' && ptr[i + 1] == '\n' &&
                ptr[i + 2] == '\r' && ptr[i + 3] == '\n') {
            head = ptr;
            hlen = i;
            ptr += i + 4;
            size -= i + 4;
            break;
        } else if (i < size - 2 && ptr[i] == '\n' && ptr[i + 1] == '\n') {
            head = ptr;
            hlen = i;
            ptr += i + 2;
            size -= i + 2;
            break;
        }
    }
    if (head && headers) {
        char *hbuf;
        TCMALLOC(hbuf, hlen + 1);
        int wi = 0;
        for (int i = 0; i < hlen; i++) {
            if (head[i] == '\r') continue;
            if (i < hlen - 1 && head[i] == '\n' && (head[i + 1] == ' ' || head[i + 1] == '\t')) {
                hbuf[wi++] = ' ';
                i++;
            } else {
                hbuf[wi++] = head[i];
            }
        }
        hbuf[wi] = '\0';
        TCLIST *list = tcstrsplit(hbuf, "\n");
        int ln = TCLISTNUM(list);
        for (int i = 0; i < ln; i++) {
            const char *line = TCLISTVALPTR(list, i);
            const char *pv = strchr(line, ':');
            if (pv) {
                char *name;
                TCMEMDUP(name, line, pv - line);
                for (int j = 0; name[j] != '\0'; j++) {
                    if (name[j] >= 'A' && name[j] <= 'Z') name[j] -= 'A' - 'a';
                }
                pv++;
                while (*pv == ' ' || *pv == '\t') {
                    pv++;
                }
                tcmapput2(headers, name, pv);
                TCFREE(name);
            }
        }
        tclistdel(list);
        TCFREE(hbuf);
        const char *pv = tcmapget2(headers, "content-type");
        if (pv) {
            const char *ep = strchr(pv, ';');
            if (ep) {
                tcmapput(headers, "TYPE", 4, pv, ep - pv);
                do {
                    ep++;
                    while (ep[0] == ' ') {
                        ep++;
                    }
                    if (tcstrifwm(ep, "charset=")) {
                        ep += 8;
                        while (*ep > '\0' && *ep <= ' ') {
                            ep++;
                        }
                        if (ep[0] == '"') ep++;
                        pv = ep;
                        while (ep[0] != '\0' && ep[0] != ' ' && ep[0] != '"' && ep[0] != ';') {
                            ep++;
                        }
                        tcmapput(headers, "CHARSET", 7, pv, ep - pv);
                    } else if (tcstrifwm(ep, "boundary=")) {
                        ep += 9;
                        while (*ep > '\0' && *ep <= ' ') {
                            ep++;
                        }
                        if (ep[0] == '"') {
                            ep++;
                            pv = ep;
                            while (ep[0] != '\0' && ep[0] != '"') {
                                ep++;
                            }
                        } else {
                            pv = ep;
                            while (ep[0] != '\0' && ep[0] != ' ' && ep[0] != '"' && ep[0] != ';') {
                                ep++;
                            }
                        }
                        tcmapput(headers, "BOUNDARY", 8, pv, ep - pv);
                    }
                } while ((ep = strchr(ep, ';')) != NULL);
            } else {
                tcmapput(headers, "TYPE", 4, pv, strlen(pv));
            }
        }
        if ((pv = tcmapget2(headers, "content-disposition")) != NULL) {
            char *ep = strchr(pv, ';');
            if (ep) {
                tcmapput(headers, "DISPOSITION", 11, pv, ep - pv);
                do {
                    ep++;
                    while (ep[0] == ' ') {
                        ep++;
                    }
                    if (tcstrifwm(ep, "filename=")) {
                        ep += 9;
                        if (ep[0] == '"') ep++;
                        pv = ep;
                        while (ep[0] != '\0' && ep[0] != '"') {
                            ep++;
                        }
                        tcmapput(headers, "FILENAME", 8, pv, ep - pv);
                    } else if (tcstrifwm(ep, "name=")) {
                        ep += 5;
                        if (ep[0] == '"') ep++;
                        pv = ep;
                        while (ep[0] != '\0' && ep[0] != '"') {
                            ep++;
                        }
                        tcmapput(headers, "NAME", 4, pv, ep - pv);
                    }
                } while ((ep = strchr(ep, ';')) != NULL);
            } else {
                tcmapput(headers, "DISPOSITION", 11, pv, strlen(pv));
            }
        }
    }
    *sp = size;
    char *rv;
    TCMEMDUP(rv, ptr, size);
    return rv;
}

/* Split multipart data of MIME into its parts. */
TCLIST *tcmimeparts(const char *ptr, int size, const char *boundary) {
    assert(ptr && size >= 0 && boundary);
    TCLIST *list = tclistnew();
    int blen = strlen(boundary);
    if (blen < 1) return list;
    const char *pv = NULL;
    for (int i = 0; i < size; i++) {
        if (ptr[i] == '-' && ptr[i + 1] == '-' && i + 2 + blen < size &&
                tcstrfwm(ptr + i + 2, boundary) && strchr("\t\n\v\f\r ", ptr[i + 2 + blen])) {
            pv = ptr + i + 2 + blen;
            if (*pv == '\r') pv++;
            if (*pv == '\n') pv++;
            size -= pv - ptr;
            ptr = pv;
            break;
        }
    }
    if (!pv) return list;
    for (int i = 0; i < size; i++) {
        if (ptr[i] == '-' && ptr[i + 1] == '-' && i + 2 + blen < size &&
                tcstrfwm(ptr + i + 2, boundary) && strchr("\t\n\v\f\r -", ptr[i + 2 + blen])) {
            const char *ep = ptr + i;
            if (ep > ptr && ep[-1] == '\n') ep--;
            if (ep > ptr && ep[-1] == '\r') ep--;
            if (ep > pv) TCLISTPUSH(list, pv, ep - pv);
            pv = ptr + i + 2 + blen;
            if (*pv == '\r') pv++;
            if (*pv == '\n') pv++;
        }
    }
    return list;
}

/* Encode a serial object with hexadecimal encoding. */
char *tchexencode(const char *ptr, int size) {
    assert(ptr && size >= 0);
    const unsigned char *rp = (const unsigned char *) ptr;
    char *buf;
    TCMALLOC(buf, size * 2 + 1);
    char *wp = buf;
    for (int i = 0; i < size; i++) {
        wp += sprintf(wp, "%02x", rp[i]);
    }
    *wp = '\0';
    return buf;
}

/* Decode a string encoded with hexadecimal encoding. */
char *tchexdecode(const char *str, int *sp) {
    assert(str && sp);
    int len = strlen(str);
    char *buf;
    TCMALLOC(buf, len + 1);
    char *wp = buf;
    for (int i = 0; i < len; i += 2) {
        while (str[i] >= '\0' && str[i] <= ' ') {
            i++;
        }
        int num = 0;
        int c = str[i];
        if (c == '\0') break;
        if (c >= '0' && c <= '9') {
            num = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            num = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            num = c - 'A' + 10;
        } else if (c == '\0') {
            break;
        }
        c = str[i + 1];
        if (c >= '0' && c <= '9') {
            num = num * 0x10 + c - '0';
        } else if (c >= 'a' && c <= 'f') {
            num = num * 0x10 + c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            num = num * 0x10 + c - 'A' + 10;
        } else if (c == '\0') {
            break;
        }
        *(wp++) = num;
    }
    *wp = '\0';
    *sp = wp - buf;
    return buf;
}

/* Compress a serial object with Packbits encoding. */
char *tcpackencode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    char *buf;
    TCMALLOC(buf, size * 2 + 1);
    char *wp = buf;
    const char *end = ptr + size;
    while (ptr < end) {
        char *sp = wp;
        const char *rp = ptr + 1;
        int step = 1;
        while (rp < end && step < 0x7f && *rp == *ptr) {
            step++;
            rp++;
        }
        if (step <= 1 && rp < end) {
            wp = sp + 1;
            *(wp++) = *ptr;
            while (rp < end && step < 0x7f && *rp != *(rp - 1)) {
                *(wp++) = *rp;
                step++;
                rp++;
            }
            if (rp < end && *(rp - 1) == *rp) {
                wp--;
                rp--;
                step--;
            }
            *sp = step == 1 ? 1 : -step;
        } else {
            *(wp++) = step;
            *(wp++) = *ptr;
        }
        ptr += step;
    }
    *sp = wp - buf;
    return buf;
}

/* Decompress a serial object compressed with Packbits encoding. */
char *tcpackdecode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    int asiz = size * 3;
    char *buf;
    TCMALLOC(buf, asiz + 1);
    int wi = 0;
    const char *end = ptr + size;
    while (ptr < end) {
        int step = abs(*ptr);
        if (wi + step >= asiz) {
            asiz = asiz * 2 + step;
            TCREALLOC(buf, buf, asiz + 1);
        }
        if (*(ptr++) >= 0) {
            memset(buf + wi, *ptr, step);
            ptr++;
        } else {
            step = tclmin(step, end - ptr);
            memcpy(buf + wi, ptr, step);
            ptr += step;
        }
        wi += step;
    }
    buf[wi] = '\0';
    *sp = wi;
    return buf;
}

/* Compress a serial object with Deflate encoding. */
char *tcdeflate(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_deflate) return NULL;
    return _tc_deflate(ptr, size, sp, _TCZMZLIB);
}

/* Decompress a serial object compressed with Deflate encoding. */
char *tcinflate(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_inflate) return NULL;
    return _tc_inflate(ptr, size, sp, _TCZMZLIB);
}

/* Compress a serial object with GZIP encoding. */
char *tcgzipencode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_deflate) return NULL;
    return _tc_deflate(ptr, size, sp, _TCZMGZIP);
}

/* Decompress a serial object compressed with GZIP encoding. */
char *tcgzipdecode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_inflate) return NULL;
    return _tc_inflate(ptr, size, sp, _TCZMGZIP);
}

/* Get the CRC32 checksum of a serial object. */
unsigned int tcgetcrc(const char *ptr, int size) {
    assert(ptr && size >= 0);
    if (!_tc_getcrc) return 0;
    return _tc_getcrc(ptr, size);
}

/* Compress a serial object with BZIP2 encoding. */
char *tcbzipencode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_bzcompress) return NULL;
    return _tc_bzcompress(ptr, size, sp);
}

/* Decompress a serial object compressed with BZIP2 encoding. */
char *tcbzipdecode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    if (!_tc_bzdecompress) return NULL;
    return _tc_bzdecompress(ptr, size, sp);
}

/* Encode an array of nonnegative integers with BER encoding. */
char *tcberencode(const unsigned int *ary, int anum, int *sp) {
    assert(ary && anum >= 0 && sp);
    char *buf;
    TCMALLOC(buf, anum * (sizeof (int) + 1) + 1);
    char *wp = buf;
    for (int i = 0; i < anum; i++) {
        unsigned int num = ary[i];
        if (num < (1 << 7)) {
            *(wp++) = num;
        } else if (num < (1 << 14)) {
            *(wp++) = (num >> 7) | 0x80;
            *(wp++) = num & 0x7f;
        } else if (num < (1 << 21)) {
            *(wp++) = (num >> 14) | 0x80;
            *(wp++) = ((num >> 7) & 0x7f) | 0x80;
            *(wp++) = num & 0x7f;
        } else if (num < (1 << 28)) {
            *(wp++) = (num >> 21) | 0x80;
            *(wp++) = ((num >> 14) & 0x7f) | 0x80;
            *(wp++) = ((num >> 7) & 0x7f) | 0x80;
            *(wp++) = num & 0x7f;
        } else {
            *(wp++) = (num >> 28) | 0x80;
            *(wp++) = ((num >> 21) & 0x7f) | 0x80;
            *(wp++) = ((num >> 14) & 0x7f) | 0x80;
            *(wp++) = ((num >> 7) & 0x7f) | 0x80;
            *(wp++) = num & 0x7f;
        }
    }
    *sp = wp - buf;
    return buf;
}

/* Decode a serial object encoded with BER encoding. */
unsigned int *tcberdecode(const char *ptr, int size, int *np) {
    assert(ptr && size >= 0 && np);
    unsigned int *buf;
    TCMALLOC(buf, size * sizeof (*buf) + 1);
    unsigned int *wp = buf;
    while (size > 0) {
        unsigned int num = 0;
        int c;
        do {
            c = *(unsigned char *) ptr;
            num = num * 0x80 + (c & 0x7f);
            ptr++;
            size--;
        } while (c >= 0x80 && size > 0);
        *(wp++) = num;
    }
    *np = wp - buf;
    return buf;
}

/* Escape meta characters in a string with the entity references of XML. */
char *tcxmlescape(const char *str) {
    assert(str);
    const char *rp = str;
    int bsiz = 0;
    while (*rp != '\0') {
        switch (*rp) {
            case '&':
                bsiz += 5;
                break;
            case '<':
                bsiz += 4;
                break;
            case '>':
                bsiz += 4;
                break;
            case '"':
                bsiz += 6;
                break;
            default:
                bsiz++;
                break;
        }
        rp++;
    }
    char *buf;
    TCMALLOC(buf, bsiz + 1);
    char *wp = buf;
    while (*str != '\0') {
        switch (*str) {
            case '&':
                memcpy(wp, "&amp;", 5);
                wp += 5;
                break;
            case '<':
                memcpy(wp, "&lt;", 4);
                wp += 4;
                break;
            case '>':
                memcpy(wp, "&gt;", 4);
                wp += 4;
                break;
            case '"':
                memcpy(wp, "&quot;", 6);
                wp += 6;
                break;
            default:
                *(wp++) = *str;
                break;
        }
        str++;
    }
    *wp = '\0';
    return buf;
}

/* Unescape entity references in a string of XML. */
char *tcxmlunescape(const char *str) {
    assert(str);
    char *buf;
    TCMALLOC(buf, strlen(str) + 1);
    char *wp = buf;
    while (*str != '\0') {
        if (*str == '&') {
            if (tcstrfwm(str, "&amp;")) {
                *(wp++) = '&';
                str += 5;
            } else if (tcstrfwm(str, "&lt;")) {
                *(wp++) = '<';
                str += 4;
            } else if (tcstrfwm(str, "&gt;")) {
                *(wp++) = '>';
                str += 4;
            } else if (tcstrfwm(str, "&quot;")) {
                *(wp++) = '"';
                str += 6;
            } else {
                *(wp++) = *(str++);
            }
        } else {
            *(wp++) = *(str++);
        }
    }
    *wp = '\0';
    return buf;
}



/*************************************************************************************************
 * encoding utilities (for experts)
 *************************************************************************************************/

/* Encode a map object into a string in the x-www-form-urlencoded format. */
char *tcwwwformencode(const TCMAP *params) {
    assert(params);
    TCXSTR *xstr = tcxstrnew3(tcmaprnum(params) * TCXSTRUNIT * 3 + 1);
    TCMAPREC *cur = params->cur;
    tcmapiterinit((TCMAP *) params);
    const char *kbuf;
    int ksiz;
    while ((kbuf = tcmapiternext((TCMAP *) params, &ksiz)) != NULL) {
        int vsiz;
        const char *vbuf = tcmapiterval(kbuf, &vsiz);
        char *kenc = tcurlencode(kbuf, ksiz);
        char *venc = tcurlencode(vbuf, vsiz);
        if (TCXSTRSIZE(xstr) > 0) TCXSTRCAT(xstr, "&", 1);
        tcxstrcat2(xstr, kenc);
        TCXSTRCAT(xstr, "=", 1);
        tcxstrcat2(xstr, venc);
        TCFREE(venc);
        TCFREE(kenc);
    }
    ((TCMAP *) params)->cur = cur;
    return tcxstrtomalloc(xstr);
}

/* Decode a query string in the x-www-form-urlencoded format. */
void tcwwwformdecode(const char *str, TCMAP *params) {
    assert(str && params);
    tcwwwformdecode2(str, strlen(str), NULL, params);
}

/* Decode a data region in the x-www-form-urlencoded or multipart-form-data format. */
void tcwwwformdecode2(const void *ptr, int size, const char *type, TCMAP *params) {
    assert(ptr && size >= 0 && params);
    if (type && tcstrfwm(tcstrskipspc(type), "multipart/")) {
        const char *brd = strstr(type, "boundary=");
        if (brd) {
            brd += 9;
            if (*brd == '"') brd++;
            char *bstr = tcstrdup(brd);
            char *wp = strchr(bstr, ';');
            if (wp) *wp = '\0';
            wp = strchr(bstr, '"');
            if (wp) *wp = '\0';
            TCLIST *parts = tcmimeparts(ptr, size, bstr);
            int pnum = tclistnum(parts);
            for (int i = 0; i < pnum; i++) {
                int psiz;
                const char *part = tclistval(parts, i, &psiz);
                TCMAP *hmap = tcmapnew2(TCMAPTINYBNUM);
                int bsiz;
                char *body = tcmimebreak(part, psiz, hmap, &bsiz);
                int nsiz;
                const char *name = tcmapget(hmap, "NAME", 4, &nsiz);
                char numbuf[TCNUMBUFSIZ];
                if (!name) {
                    nsiz = sprintf(numbuf, "part:%d", i + 1);
                    name = numbuf;
                }
                const char *tenc = tcmapget2(hmap, "content-transfer-encoding");
                if (tenc) {
                    if (tcstrifwm(tenc, "base64")) {
                        char *ebuf = tcbasedecode(body, &bsiz);
                        TCFREE(body);
                        body = ebuf;
                    } else if (tcstrifwm(tenc, "quoted-printable")) {
                        char *ebuf = tcquotedecode(body, &bsiz);
                        TCFREE(body);
                        body = ebuf;
                    }
                }
                tcmapputkeep(params, name, nsiz, body, bsiz);
                const char *fname = tcmapget2(hmap, "FILENAME");
                if (fname) {
                    if (*fname == '/') {
                        fname = strrchr(fname, '/') + 1;
                    } else if (((*fname >= 'a' && *fname <= 'z') || (*fname >= 'A' && *fname <= 'Z')) &&
                            fname[1] == ':' && fname[2] == '\\') {
                        fname = strrchr(fname, '\\') + 1;
                    }
                    if (*fname != '\0') {
                        char key[nsiz + 10];
                        sprintf(key, "%s_filename", name);
                        tcmapput2(params, key, fname);
                    }
                }
                tcfree(body);
                tcmapdel(hmap);
            }
            tclistdel(parts);
            tcfree(bstr);
        }
    } else {
        const char *rp = ptr;
        const char *pv = rp;
        const char *ep = rp + size;
        char stack[TCIOBUFSIZ];
        while (rp < ep) {
            if (*rp == '&' || *rp == ';') {
                while (pv < rp && *pv > '\0' && *pv <= ' ') {
                    pv++;
                }
                if (rp > pv) {
                    int len = rp - pv;
                    char *rbuf;
                    if (len < sizeof (stack)) {
                        rbuf = stack;
                    } else {
                        TCMALLOC(rbuf, len + 1);
                    }
                    memcpy(rbuf, pv, len);
                    rbuf[len] = '\0';
                    char *sep = strchr(rbuf, '=');
                    if (sep) {
                        *(sep++) = '\0';
                    } else {
                        sep = "";
                    }
                    int ksiz;
                    char *kbuf = tcurldecode(rbuf, &ksiz);
                    int vsiz;
                    char *vbuf = tcurldecode(sep, &vsiz);
                    if (!tcmapputkeep(params, kbuf, ksiz, vbuf, vsiz)) {
                        tcmapputcat(params, kbuf, ksiz, "", 1);
                        tcmapputcat(params, kbuf, ksiz, vbuf, vsiz);
                    }
                    TCFREE(vbuf);
                    TCFREE(kbuf);
                    if (rbuf != stack) TCFREE(rbuf);
                }
                pv = rp + 1;
            }
            rp++;
        }
        while (pv < rp && *pv > '\0' && *pv <= ' ') {
            pv++;
        }
        if (rp > pv) {
            int len = rp - pv;
            char *rbuf;
            if (len < sizeof (stack)) {
                rbuf = stack;
            } else {
                TCMALLOC(rbuf, len + 1);
            }
            memcpy(rbuf, pv, len);
            rbuf[len] = '\0';
            char *sep = strchr(rbuf, '=');
            if (sep) {
                *(sep++) = '\0';
            } else {
                sep = "";
            }
            int ksiz;
            char *kbuf = tcurldecode(rbuf, &ksiz);
            int vsiz;
            char *vbuf = tcurldecode(sep, &vsiz);
            if (!tcmapputkeep(params, kbuf, ksiz, vbuf, vsiz)) {
                tcmapputcat(params, kbuf, ksiz, "", 1);
                tcmapputcat(params, kbuf, ksiz, vbuf, vsiz);
            }
            TCFREE(vbuf);
            TCFREE(kbuf);
            if (rbuf != stack) TCFREE(rbuf);
        }
    }
}

/* Split an XML string into tags and text sections. */
TCLIST *tcxmlbreak(const char *str) {
    assert(str);
    TCLIST *list = tclistnew();
    int i = 0;
    int pv = 0;
    bool tag = false;
    char *ep;
    while (true) {
        if (str[i] == '\0') {
            if (i > pv) TCLISTPUSH(list, str + pv, i - pv);
            break;
        } else if (!tag && str[i] == '<') {
            if (str[i + 1] == '!' && str[i + 2] == '-' && str[i + 3] == '-') {
                if (i > pv) TCLISTPUSH(list, str + pv, i - pv);
                if ((ep = strstr(str + i, "-->")) != NULL) {
                    TCLISTPUSH(list, str + i, ep - str - i + 3);
                    i = ep - str + 2;
                    pv = i + 1;
                }
            } else if (str[i + 1] == '!' && str[i + 2] == '[' && tcstrifwm(str + i, "<![CDATA[")) {
                if (i > pv) TCLISTPUSH(list, str + pv, i - pv);
                if ((ep = strstr(str + i, "]]>")) != NULL) {
                    i += 9;
                    TCXSTR *xstr = tcxstrnew();
                    while (str + i < ep) {
                        if (str[i] == '&') {
                            TCXSTRCAT(xstr, "&amp;", 5);
                        } else if (str[i] == '<') {
                            TCXSTRCAT(xstr, "&lt;", 4);
                        } else if (str[i] == '>') {
                            TCXSTRCAT(xstr, "&gt;", 4);
                        } else {
                            TCXSTRCAT(xstr, str + i, 1);
                        }
                        i++;
                    }
                    if (TCXSTRSIZE(xstr) > 0) TCLISTPUSH(list, TCXSTRPTR(xstr), TCXSTRSIZE(xstr));
                    tcxstrdel(xstr);
                    i = ep - str + 2;
                    pv = i + 1;
                }
            } else {
                if (i > pv) TCLISTPUSH(list, str + pv, i - pv);
                tag = true;
                pv = i;
            }
        } else if (tag && str[i] == '>') {
            if (i > pv) TCLISTPUSH(list, str + pv, i - pv + 1);
            tag = false;
            pv = i + 1;
        }
        i++;
    }
    return list;
}

/* Get the map of attributes of an XML tag. */
TCMAP *tcxmlattrs(const char *str) {
    assert(str);
    TCMAP *map = tcmapnew2(TCXMLATBNUM);
    const unsigned char *rp = (unsigned char *) str;
    while (*rp == '<' || *rp == '/' || *rp == '?' || *rp == '!' || *rp == ' ') {
        rp++;
    }
    const unsigned char *key = rp;
    while (*rp > 0x20 && *rp != '/' && *rp != '>') {
        rp++;
    }
    tcmapputkeep(map, "", 0, (char *) key, rp - key);
    while (*rp != '\0') {
        while (*rp != '\0' && (*rp <= 0x20 || *rp == '/' || *rp == '?' || *rp == '>')) {
            rp++;
        }
        key = rp;
        while (*rp > 0x20 && *rp != '/' && *rp != '>' && *rp != '=') {
            rp++;
        }
        int ksiz = rp - key;
        while (*rp != '\0' && (*rp == '=' || *rp <= 0x20)) {
            rp++;
        }
        const unsigned char *val;
        int vsiz;
        if (*rp == '"') {
            rp++;
            val = rp;
            while (*rp != '\0' && *rp != '"') {
                rp++;
            }
            vsiz = rp - val;
        } else if (*rp == '\'') {
            rp++;
            val = rp;
            while (*rp != '\0' && *rp != '\'') {
                rp++;
            }
            vsiz = rp - val;
        } else {
            val = rp;
            while (*rp > 0x20 && *rp != '"' && *rp != '\'' && *rp != '>') {
                rp++;
            }
            vsiz = rp - val;
        }
        if (*rp != '\0') rp++;
        if (ksiz > 0) {
            char *copy;
            TCMEMDUP(copy, val, vsiz);
            char *raw = tcxmlunescape(copy);
            tcmapputkeep(map, (char *) key, ksiz, raw, strlen(raw));
            TCFREE(raw);
            TCFREE(copy);
        }
    }
    return map;
}

/* Escape meta characters in a string with backslash escaping of the C language. */
char *tccstrescape(const char *str) {
    assert(str);
    int asiz = TCXSTRUNIT * 2;
    char *buf;
    TCMALLOC(buf, asiz + 4);
    int wi = 0;
    bool hex = false;
    int c;
    while ((c = *(unsigned char *) str) != '\0') {
        if (wi >= asiz) {
            asiz *= 2;
            TCREALLOC(buf, buf, asiz + 4);
        }
        if (c < ' ' || c == 0x7f || c == '"' || c == '\'' || c == '\\') {
            switch (c) {
                case '\t': wi += sprintf(buf + wi, "\\t");
                    break;
                case '\n': wi += sprintf(buf + wi, "\\n");
                    break;
                case '\r': wi += sprintf(buf + wi, "\\r");
                    break;
                case '\\': wi += sprintf(buf + wi, "\\\\");
                    break;
                default:
                    wi += sprintf(buf + wi, "\\x%02X", c);
                    hex = true;
                    break;
            }
        } else {
            if (hex && ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                wi += sprintf(buf + wi, "\\x%02X", c);
                hex = true;
            } else {
                buf[wi++] = c;
                hex = false;
            }
        }
        str++;
    }
    buf[wi] = '\0';
    return buf;
}

/* Unescape a string escaped by backslash escaping of the C language. */
char *tccstrunescape(const char *str) {
    assert(str);
    int asiz = TCXSTRUNIT * 2;
    char *buf;
    TCMALLOC(buf, asiz + 4);
    int wi = 0;
    int c;
    while ((c = *(unsigned char *) str) != '\0') {
        if (wi >= asiz) {
            asiz *= 2;
            TCREALLOC(buf, buf, asiz + 4);
        }
        if (c == '\\' && str[1] != '\0') {
            str++;
            int si = wi;
            switch (*str) {
                case 'a': buf[wi++] = '\a';
                    break;
                case 'b': buf[wi++] = '\b';
                    break;
                case 't': buf[wi++] = '\t';
                    break;
                case 'n': buf[wi++] = '\n';
                    break;
                case 'v': buf[wi++] = '\v';
                    break;
                case 'f': buf[wi++] = '\f';
                    break;
                case 'r': buf[wi++] = '\r';
                    break;
            }
            if (si == wi) {
                c = *str;
                if (c == 'x') {
                    str++;
                    int code = 0;
                    for (int i = 0; i < 2; i++) {
                        c = *str;
                        if (c >= '0' && c <= '9') {
                            code = code * 0x10 + c - '0';
                        } else if (c >= 'A' && c <= 'F') {
                            code = code * 0x10 + c - 'A' + 10;
                        } else if (c >= 'a' && c <= 'f') {
                            code = code * 0x10 + c - 'a' + 10;
                        } else {
                            break;
                        }
                        str++;
                    }
                    buf[wi++] = code;
                } else if (c == 'u' || c == 'U') {
                    int len = (c == 'U') ? 8 : 4;
                    str++;
                    int code = 0;
                    for (int i = 0; i < len; i++) {
                        c = *str;
                        if (c >= '0' && c <= '9') {
                            code = code * 0x10 + c - '0';
                        } else if (c >= 'A' && c <= 'F') {
                            code = code * 0x10 + c - 'A' + 10;
                        } else if (c >= 'a' && c <= 'f') {
                            code = code * 0x10 + c - 'a' + 10;
                        } else {
                            break;
                        }
                        str++;
                    }
                    uint16_t ary[1];
                    ary[0] = code;
                    wi += tcstrucstoutf(ary, 1, buf + wi);
                } else if (c >= '0' && c <= '8') {
                    int code = 0;
                    for (int i = 0; i < 3; i++) {
                        c = *str;
                        if (c >= '0' && c <= '7') {
                            code = code * 8 + c - '0';
                        } else {
                            break;
                        }
                        str++;
                    }
                    buf[wi++] = code;
                } else if (c != '\0') {
                    buf[wi++] = c;
                    str++;
                }
            } else {
                str++;
            }
        } else {
            buf[wi++] = c;
            str++;
        }
    }
    buf[wi] = '\0';
    return buf;
}

/* Escape meta characters in a string with backslash escaping of JSON. */
char *tcjsonescape(const char *str) {
    assert(str);
    int asiz = TCXSTRUNIT * 2;
    char *buf;
    TCMALLOC(buf, asiz + 6);
    int wi = 0;
    int c;
    while ((c = *(unsigned char *) str) != '\0') {
        if (wi >= asiz) {
            asiz *= 2;
            TCREALLOC(buf, buf, asiz + 6);
        }
        if (c < ' ' || c == 0x7f || c == '"' || c == '\'' || c == '\\') {
            switch (c) {
                case '\t': wi += sprintf(buf + wi, "\\t");
                    break;
                case '\n': wi += sprintf(buf + wi, "\\n");
                    break;
                case '\r': wi += sprintf(buf + wi, "\\r");
                    break;
                case '\\': wi += sprintf(buf + wi, "\\\\");
                    break;
                default: wi += sprintf(buf + wi, "\\u%04X", c);
                    break;
            }
        } else {
            buf[wi++] = c;
        }
        str++;
    }
    buf[wi] = '\0';
    return buf;
}

/* Unescape a string escaped by backslash escaping of JSON. */
char *tcjsonunescape(const char *str) {
    assert(str);
    return tccstrunescape(str);
}



/*************************************************************************************************
 * template serializer
 *************************************************************************************************/


#define TCTMPLUNIT     65536             // allocation unit size of a template string
#define TCTMPLMAXDEP   256               // maximum depth of template blocks
#define TCTMPLBEGSEP   "[%"              // default beginning separator
#define TCTMPLENDSEP   "%]"              // default beginning separator
#define TCTYPRFXLIST   "[list]\0:"       // type prefix for a list object
#define TCTYPRFXMAP    "[map]\0:"        // type prefix for a list object


/* private function prototypes */
static TCLIST *tctmpltokenize(const char *expr);
static int tctmpldumpeval(TCXSTR *xstr, const char *expr, const TCLIST *elems, int cur, int num,
        const TCMAP **stack, int depth);
static const char *tctmpldumpevalvar(const TCMAP **stack, int depth, const char *name,
        int *sp, int *np);

/* Create a template object. */
TCTMPL *tctmplnew(void) {
    TCTMPL *tmpl;
    TCMALLOC(tmpl, sizeof (*tmpl));
    tmpl->elems = NULL;
    tmpl->begsep = NULL;
    tmpl->endsep = NULL;
    tmpl->conf = tcmapnew2(TCMAPTINYBNUM);
    return tmpl;
}

/* Delete a template object. */
void tctmpldel(TCTMPL *tmpl) {
    assert(tmpl);
    tcmapdel(tmpl->conf);
    if (tmpl->endsep) TCFREE(tmpl->endsep);
    if (tmpl->begsep) TCFREE(tmpl->begsep);
    if (tmpl->elems) tclistdel(tmpl->elems);
    TCFREE(tmpl);
}

/* Set the separator strings of a template object. */
void tctmplsetsep(TCTMPL *tmpl, const char *begsep, const char *endsep) {
    assert(tmpl && begsep && endsep);
    if (tmpl->endsep) TCFREE(tmpl->endsep);
    if (tmpl->begsep) TCFREE(tmpl->begsep);
    tmpl->begsep = tcstrdup(begsep);
    tmpl->endsep = tcstrdup(endsep);
}

/* Load a template string into a template object. */
void tctmplload(TCTMPL *tmpl, const char *str) {
    assert(tmpl && str);
    const char *begsep = tmpl->begsep;
    if (!begsep) begsep = TCTMPLBEGSEP;
    const char *endsep = tmpl->endsep;
    if (!endsep) endsep = TCTMPLENDSEP;
    int beglen = strlen(begsep);
    int endlen = strlen(endsep);
    if (beglen < 1 || endlen < 1) return;
    int begchr = *begsep;
    int endchr = *endsep;
    if (tmpl->elems) tclistdel(tmpl->elems);
    tcmapclear(tmpl->conf);
    TCLIST *elems = tclistnew();
    const char *rp = str;
    const char *pv = rp;
    while (*rp != '\0') {
        if (*rp == begchr && tcstrfwm(rp, begsep)) {
            if (rp > pv) TCLISTPUSH(elems, pv, rp - pv);
            rp += beglen;
            pv = rp;
            while (*rp != '\0') {
                if (*rp == endchr && tcstrfwm(rp, endsep)) {
                    bool chop = false;
                    while (pv < rp && *pv > '\0' && *pv <= ' ') {
                        pv++;
                    }
                    if (*pv == '"') {
                        pv++;
                        const char *sp = pv;
                        while (pv < rp && *pv != '"') {
                            pv++;
                        }
                        if (pv > sp) TCLISTPUSH(elems, sp, pv - sp);
                    } else if (*pv == '\'') {
                        pv++;
                        const char *sp = pv;
                        while (pv < rp && *pv != '\'') {
                            pv++;
                        }
                        if (pv > sp) TCLISTPUSH(elems, sp, pv - sp);
                    } else {
                        const char *ep = rp;
                        if (ep > pv && ep[-1] == '\\') {
                            ep--;
                            chop = true;
                        }
                        while (ep > pv && ((unsigned char *) ep)[-1] <= ' ') {
                            ep--;
                        }
                        int len = ep - pv;
                        char *buf;
                        TCMALLOC(buf, len + 1);
                        *buf = '\0';
                        memcpy(buf + 1, pv, len);
                        tclistpushmalloc(elems, buf, len + 1);
                        if (tcstrfwm(pv, "CONF")) {
                            const char *expr = (char *) TCLISTVALPTR(elems, TCLISTNUM(elems) - 1) + 1;
                            TCLIST *tokens = tctmpltokenize(expr);
                            int tnum = TCLISTNUM(tokens);
                            if (tnum > 1 && !strcmp(TCLISTVALPTR(tokens, 0), "CONF")) {
                                const char *name = TCLISTVALPTR(tokens, 1);
                                const char *value = (tnum > 2) ? TCLISTVALPTR(tokens, 2) : "";
                                tcmapput2(tmpl->conf, name, value);
                            }
                            tclistdel(tokens);
                        }
                    }
                    rp += endlen;
                    if (chop) {
                        if (*rp == '\r') rp++;
                        if (*rp == '\n') rp++;
                    }
                    break;
                }
                rp++;
            }
            pv = rp;
        } else {
            rp++;
        }
    }
    if (rp > pv) TCLISTPUSH(elems, pv, rp - pv);
    tmpl->elems = elems;
}

/* Load a template string from a file into a template object. */
bool tctmplload2(TCTMPL *tmpl, const char *path) {
    assert(tmpl && path);
    char *str = tcreadfile(path, -1, NULL);
    if (!str) return false;
    tctmplload(tmpl, str);
    TCFREE(str);
    return true;
}

/* Serialize the template string of a template object. */
char *tctmpldump(TCTMPL *tmpl, const TCMAP *vars) {
    assert(tmpl && vars);
    TCXSTR *xstr = tcxstrnew3(TCTMPLUNIT);
    TCLIST *elems = tmpl->elems;
    if (elems) {
        TCMAP *svars = tcmapnew2(TCMAPTINYBNUM);
        int cur = 0;
        int num = TCLISTNUM(elems);
        const TCMAP * stack[TCTMPLMAXDEP];
        int depth = 0;
        stack[depth++] = tmpl->conf;
        stack[depth++] = svars;
        stack[depth++] = vars;
        while (cur < num) {
            const char *elem;
            int esiz;
            TCLISTVAL(elem, elems, cur, esiz);
            if (*elem == '\0' && esiz > 0) {
                cur = tctmpldumpeval(xstr, elem + 1, elems, cur, num, stack, depth);
            } else {
                TCXSTRCAT(xstr, elem, esiz);
                cur++;
            }
        }
        tcmapdel(svars);
    }
    return tcxstrtomalloc(xstr);
}

/* Get the value of a configuration variable of a template object. */
const char *tctmplconf(TCTMPL *tmpl, const char *name) {
    assert(tmpl && name);
    return tcmapget2(tmpl->conf, name);
}

/* Store a list object into a list object with the type information. */
void tclistpushlist(TCLIST *list, const TCLIST *obj) {
    assert(list && obj);
    char vbuf[sizeof (TCTYPRFXLIST) - 1 + sizeof (obj)];
    memcpy(vbuf, TCTYPRFXLIST, sizeof (TCTYPRFXLIST) - 1);
    memcpy(vbuf + sizeof (TCTYPRFXLIST) - 1, &obj, sizeof (obj));
    tclistpush(list, vbuf, sizeof (vbuf));
}

/* Store a map object into a list object with the type information. */
void tclistpushmap(TCLIST *list, const TCMAP *obj) {
    assert(list && obj);
    char vbuf[sizeof (TCTYPRFXMAP) - 1 + sizeof (obj)];
    memcpy(vbuf, TCTYPRFXMAP, sizeof (TCTYPRFXMAP) - 1);
    memcpy(vbuf + sizeof (TCTYPRFXMAP) - 1, &obj, sizeof (obj));
    tclistpush(list, vbuf, sizeof (vbuf));
}

/* Store a list object into a map object with the type information. */
void tcmapputlist(TCMAP *map, const char *kstr, const TCLIST *obj) {
    assert(map && kstr && obj);
    char vbuf[sizeof (TCTYPRFXLIST) - 1 + sizeof (obj)];
    memcpy(vbuf, TCTYPRFXLIST, sizeof (TCTYPRFXLIST) - 1);
    memcpy(vbuf + sizeof (TCTYPRFXLIST) - 1, &obj, sizeof (obj));
    tcmapput(map, kstr, strlen(kstr), vbuf, sizeof (vbuf));
}

/* Store a map object into a map object with the type information. */
void tcmapputmap(TCMAP *map, const char *kstr, const TCMAP *obj) {
    assert(map && kstr && obj);
    char vbuf[sizeof (TCTYPRFXMAP) - 1 + sizeof (obj)];
    memcpy(vbuf, TCTYPRFXMAP, sizeof (TCTYPRFXMAP) - 1);
    memcpy(vbuf + sizeof (TCTYPRFXMAP) - 1, &obj, sizeof (obj));
    tcmapput(map, kstr, strlen(kstr), vbuf, sizeof (vbuf));
}

/* Tokenize an template expression.
   `expr' specifies the expression.
   The return value is a list object of tokens. */
static TCLIST *tctmpltokenize(const char *expr) {
    TCLIST *tokens = tclistnew();
    const unsigned char *rp = (unsigned char *) expr;
    while (*rp != '\0') {
        while (*rp > '\0' && *rp <= ' ') {
            rp++;
        }
        const unsigned char *pv = rp;
        if (*rp == '"') {
            pv++;
            rp++;
            while (*rp != '\0' && *rp != '"') {
                rp++;
            }
            TCLISTPUSH(tokens, pv, rp - pv);
            if (*rp == '"') rp++;
        } else if (*rp == '\'') {
            pv++;
            rp++;
            while (*rp != '\0' && *rp != '\'') {
                rp++;
            }
            TCLISTPUSH(tokens, pv, rp - pv);
            if (*rp == '\'') rp++;
        } else {
            while (*rp > ' ') {
                rp++;
            }
            if (rp > pv) TCLISTPUSH(tokens, pv, rp - pv);
        }
    }
    return tokens;
}

/* Evaluate an template expression.
   `xstr' specifies the output buffer.
   `expr' specifies the expression.
   `elems' specifies the list of elements.
   `cur' specifies the current offset of the elements.
   `num' specifies the number of the elements.
   `stack' specifies the variable scope stack.
   `depth' specifies the current depth of the stack.
   The return value is the next offset of the elements. */
static int tctmpldumpeval(TCXSTR *xstr, const char *expr, const TCLIST *elems, int cur, int num,
        const TCMAP **stack, int depth) {
    assert(expr && elems && cur >= 0 && num >= 0 && stack && depth >= 0);
    cur++;
    TCLIST *tokens = tctmpltokenize(expr);
    int tnum = TCLISTNUM(tokens);
    if (tnum > 0) {
        const char *cmd = TCLISTVALPTR(tokens, 0);
        if (!strcmp(cmd, "IF")) {
            bool sign = true;
            const char *eq = NULL;
            const char *inc = NULL;
            bool prt = false;
            const char *rx = NULL;
            for (int i = 1; i < tnum; i++) {
                const char *token = TCLISTVALPTR(tokens, i);
                if (!strcmp(token, "NOT")) {
                    sign = !sign;
                } else if (!strcmp(token, "EQ")) {
                    if (++i < tnum) eq = TCLISTVALPTR(tokens, i);
                } else if (!strcmp(token, "INC")) {
                    if (++i < tnum) inc = TCLISTVALPTR(tokens, i);
                } else if (!strcmp(token, "PRT")) {
                    prt = true;
                } else if (!strcmp(token, "RX")) {
                    if (++i < tnum) rx = TCLISTVALPTR(tokens, i);
                }
            }
            TCXSTR *altxstr = NULL;
            if (xstr) {
                const char *name = (tnum > 1) ? TCLISTVALPTR(tokens, 1) : "__";
                int vsiz, vnum;
                const char *vbuf = tctmpldumpevalvar(stack, depth, name, &vsiz, &vnum);
                char numbuf[TCNUMBUFSIZ];
                if (vbuf && vnum >= 0) {
                    vsiz = sprintf(numbuf, "%d", vnum);
                    vbuf = numbuf;
                }
                bool bval = false;
                if (vbuf) {
                    if (eq) {
                        if (!strcmp(vbuf, eq)) bval = true;
                    } else if (inc) {
                        if (strstr(vbuf, inc)) bval = true;
                    } else if (prt) {
                        if (*tcstrskipspc(vbuf) != '\0') bval = true;
                    } else if (rx) {
                        if (tcregexmatch(vbuf, rx)) bval = true;
                    } else {
                        bval = true;
                    }
                }
                if (bval != sign) {
                    altxstr = xstr;
                    xstr = NULL;
                }
            }
            while (cur < num) {
                const char *elem;
                int esiz;
                TCLISTVAL(elem, elems, cur, esiz);
                if (*elem == '\0' && esiz > 0) {
                    cur = tctmpldumpeval(xstr, elem + 1, elems, cur, num, stack, depth);
                    if (!strcmp(elem + 1, "ELSE")) {
                        xstr = altxstr;
                    } else if (!strcmp(elem + 1, "END")) {
                        break;
                    }
                } else {
                    if (xstr) TCXSTRCAT(xstr, elem, esiz);
                    cur++;
                }
            }
        } else if (!strcmp(cmd, "FOREACH")) {
            const TCLIST *list = NULL;
            if (xstr) {
                const char *name = (tnum > 1) ? TCLISTVALPTR(tokens, 1) : "";
                int vsiz, vnum;
                const char *vbuf = tctmpldumpevalvar(stack, depth, name, &vsiz, &vnum);
                if (vbuf && vsiz == sizeof (TCTYPRFXLIST) - 1 + sizeof (list) &&
                        !memcmp(vbuf, TCTYPRFXLIST, sizeof (TCTYPRFXLIST) - 1)) {
                    memcpy(&list, vbuf + sizeof (TCTYPRFXLIST) - 1, sizeof (list));
                }
            }
            if (list && TCLISTNUM(list) > 0) {
                const char *name = (tnum > 2) ? TCLISTVALPTR(tokens, 2) : "";
                TCMAP *vars = tcmapnew2(1);
                if (depth < TCTMPLMAXDEP) {
                    stack[depth] = vars;
                    depth++;
                }
                int lnum = TCLISTNUM(list);
                int beg = cur;
                for (int i = 0; i < lnum; i++) {
                    const char *vbuf;
                    int vsiz;
                    TCLISTVAL(vbuf, list, i, vsiz);
                    if (vsiz == sizeof (TCTYPRFXLIST) - 1 + sizeof (TCLIST *) &&
                            !memcmp(vbuf, TCTYPRFXLIST, sizeof (TCTYPRFXLIST) - 1)) {
                        TCLIST *obj;
                        memcpy(&obj, vbuf + sizeof (TCTYPRFXLIST) - 1, sizeof (obj));
                        tcmapputlist(vars, name, obj);
                    } else if (vsiz == sizeof (TCTYPRFXMAP) - 1 + sizeof (TCMAP *) &&
                            !memcmp(vbuf, TCTYPRFXMAP, sizeof (TCTYPRFXMAP) - 1)) {
                        TCMAP *obj;
                        memcpy(&obj, vbuf + sizeof (TCTYPRFXMAP) - 1, sizeof (obj));
                        tcmapputmap(vars, name, obj);
                    } else {
                        tcmapput2(vars, name, vbuf);
                    }
                    cur = beg;
                    while (cur < num) {
                        const char *elem;
                        int esiz;
                        TCLISTVAL(elem, elems, cur, esiz);
                        if (*elem == '\0' && esiz > 0) {
                            cur = tctmpldumpeval(xstr, elem + 1, elems, cur, num, stack, depth);
                            if (!strcmp(elem + 1, "END")) break;
                        } else {
                            if (xstr) TCXSTRCAT(xstr, elem, esiz);
                            cur++;
                        }
                    }
                }
                tcmapdel(vars);
            } else {
                while (cur < num) {
                    const char *elem;
                    int esiz;
                    TCLISTVAL(elem, elems, cur, esiz);
                    if (*elem == '\0' && esiz > 0) {
                        cur = tctmpldumpeval(NULL, elem + 1, elems, cur, num, stack, depth);
                        if (!strcmp(elem + 1, "END")) break;
                    } else {
                        cur++;
                    }
                }
            }
        } else if (!strcmp(cmd, "SET")) {
            if (xstr) {
                const char *name = (tnum > 1) ? TCLISTVALPTR(tokens, 1) : "";
                const char *value = (tnum > 2) ? TCLISTVALPTR(tokens, 2) : "";
                tcmapput2((TCMAP *) stack[1], name, value);
            }
        } else if (xstr) {
            int vsiz, vnum;
            const char *vbuf = tctmpldumpevalvar(stack, depth, cmd, &vsiz, &vnum);
            char numbuf[TCNUMBUFSIZ];
            if (vbuf && vnum >= 0) {
                vsiz = sprintf(numbuf, "%d", vnum);
                vbuf = numbuf;
            }
            const char *enc = "";
            const char *def = NULL;
            for (int i = 1; i < tnum; i++) {
                const char *token = TCLISTVALPTR(tokens, i);
                if (!strcmp(token, "ENC")) {
                    if (++i < tnum) enc = TCLISTVALPTR(tokens, i);
                } else if (!strcmp(token, "DEF")) {
                    if (++i < tnum) def = TCLISTVALPTR(tokens, i);
                }
            }
            if (!vbuf && def) {
                vbuf = def;
                vsiz = strlen(def);
            }
            if (vbuf) {
                if (!strcmp(enc, "URL")) {
                    char *ebuf = tcurlencode(vbuf, vsiz);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "BASE")) {
                    char *ebuf = tcbaseencode(vbuf, vsiz);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "QUOTE")) {
                    char *ebuf = tcquoteencode(vbuf, vsiz);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "HEX")) {
                    char *ebuf = tchexencode(vbuf, vsiz);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "XML")) {
                    char *ebuf = tcxmlescape(vbuf);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "CSTR")) {
                    char *ebuf = tccstrescape(vbuf);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "JSON")) {
                    char *ebuf = tcjsonescape(vbuf);
                    tcxstrcat2(xstr, ebuf);
                    TCFREE(ebuf);
                } else if (!strcmp(enc, "MD5")) {
                    char ebuf[48];
                    tcmd5hash(vbuf, vsiz, ebuf);
                    tcxstrcat2(xstr, ebuf);
                } else {
                    tcxstrcat2(xstr, vbuf);
                }
            }
        }
    }
    tclistdel(tokens);
    return cur;
}

/* Evaluate a variable of a template expression.
   `stack' specifies the variable scope stack.
   `depth' specifies the current depth of the stack.
   `name' specifies the variable name.
   `sp' specifies the length of the region of the return value.
   `np' specifies the number information of the return value.
   The return value is the pointer to the region of the evaluated value. */
static const char *tctmpldumpevalvar(const TCMAP **stack, int depth, const char *name,
        int *sp, int *np) {
    assert(stack && depth >= 0 && name && sp && np);
    const char *result = NULL;
    TCLIST *tokens = tcstrsplit(name, ".");
    int tnum = TCLISTNUM(tokens);
    if (tnum > 0) {
        const char *token;
        int tsiz;
        TCLISTVAL(token, tokens, 0, tsiz);
        for (int i = depth - 1; i >= 0; i--) {
            const TCMAP *vars = stack[i];
            int vsiz;
            const char *vbuf = tcmapget(vars, token, tsiz, &vsiz);
            int tidx = 1;
            if (vbuf) {
                while (vbuf) {
                    if (vsiz == sizeof (TCTYPRFXLIST) - 1 + sizeof (TCLIST *) &&
                            !memcmp(vbuf, TCTYPRFXLIST, sizeof (TCTYPRFXLIST) - 1)) {
                        result = vbuf;
                        *sp = vsiz;
                        TCLIST *list;
                        memcpy(&list, vbuf + sizeof (TCTYPRFXLIST) - 1, sizeof (list));
                        *np = tclistnum(list);
                        break;
                    } else if (vsiz == sizeof (TCTYPRFXMAP) - 1 + sizeof (TCMAP *) &&
                            !memcmp(vbuf, TCTYPRFXMAP, sizeof (TCTYPRFXMAP) - 1)) {
                        if (tidx < tnum) {
                            memcpy(&vars, vbuf + sizeof (TCTYPRFXMAP) - 1, sizeof (TCMAP *));
                            TCLISTVAL(token, tokens, tidx, tsiz);
                            vbuf = tcmapget(vars, token, tsiz, &vsiz);
                            tidx++;
                        } else {
                            result = vbuf;
                            *sp = vsiz;
                            TCMAP *map;
                            memcpy(&map, vbuf + sizeof (TCTYPRFXMAP) - 1, sizeof (map));
                            *np = tcmaprnum(map);
                            break;
                        }
                    } else {
                        result = vbuf;
                        *sp = vsiz;
                        *np = -1;
                        break;
                    }
                }
                break;
            }
        }
    }
    tclistdel(tokens);
    return result;
}



/*************************************************************************************************
 * pointer list
 *************************************************************************************************/

/* Create a pointer list object. */
TCPTRLIST *tcptrlistnew(void) {
    TCPTRLIST *ptrlist;
    TCMALLOC(ptrlist, sizeof (*ptrlist));
    ptrlist->anum = TCLISTUNIT;
    TCMALLOC(ptrlist->array, sizeof (ptrlist->array[0]) * ptrlist->anum);
    ptrlist->start = 0;
    ptrlist->num = 0;
    return ptrlist;
}

/* Create a pointer list object with expecting the number of elements. */
TCPTRLIST *tcptrlistnew2(int anum) {
    TCPTRLIST *ptrlist;
    TCMALLOC(ptrlist, sizeof (*ptrlist));
    if (anum < 1) anum = 1;
    ptrlist->anum = anum;
    TCMALLOC(ptrlist->array, sizeof (ptrlist->array[0]) * ptrlist->anum);
    ptrlist->start = 0;
    ptrlist->num = 0;
    return ptrlist;
}

/* Copy a pointer list object. */
TCPTRLIST *tcptrlistdup(const TCPTRLIST *ptrlist) {
    assert(ptrlist);
    int num = ptrlist->num;
    if (num < 1) return tcptrlistnew();
    void **array = ptrlist->array + ptrlist->start;
    TCPTRLIST *nptrlist;
    TCMALLOC(nptrlist, sizeof (*nptrlist));
    void **narray;
    TCMALLOC(narray, sizeof (*narray) * num);
    memcpy(narray, array, sizeof (*narray) * num);
    nptrlist->anum = num;
    nptrlist->array = narray;
    nptrlist->start = 0;
    nptrlist->num = num;
    return nptrlist;
}

/* Delete a pointer list object. */
void tcptrlistdel(TCPTRLIST *ptrlist) {
    assert(ptrlist);
    TCFREE(ptrlist->array);
    TCFREE(ptrlist);
}

/* Get the number of elements of a pointer list object. */
int tcptrlistnum(const TCPTRLIST *ptrlist) {
    assert(ptrlist);
    return ptrlist->num;
}

/* Get the pointer to the region of an element of a pointer list object. */
void *tcptrlistval(const TCPTRLIST *ptrlist, int index) {
    assert(ptrlist && index >= 0);
    if (index >= ptrlist->num) return NULL;
    return ptrlist->array[ptrlist->start + index];
}

/* Add an element at the end of a pointer list object. */
void tcptrlistpush(TCPTRLIST *ptrlist, void *ptr) {
    assert(ptrlist && ptr);
    int index = ptrlist->start + ptrlist->num;
    if (index >= ptrlist->anum) {
        ptrlist->anum += ptrlist->num + 1;
        TCREALLOC(ptrlist->array, ptrlist->array, ptrlist->anum * sizeof (ptrlist->array[0]));
    }
    ptrlist->array[index] = ptr;
    ptrlist->num++;
}

/* Remove an element of the end of a pointer list object. */
void *tcptrlistpop(TCPTRLIST *ptrlist) {
    assert(ptrlist);
    if (ptrlist->num < 1) return NULL;
    int index = ptrlist->start + ptrlist->num - 1;
    ptrlist->num--;
    return ptrlist->array[index];
}

/* Add an element at the top of a pointer list object. */
void tcptrlistunshift(TCPTRLIST *ptrlist, void *ptr) {
    assert(ptrlist && ptr);
    if (ptrlist->start < 1) {
        if (ptrlist->start + ptrlist->num >= ptrlist->anum) {
            ptrlist->anum += ptrlist->num + 1;
            TCREALLOC(ptrlist->array, ptrlist->array, ptrlist->anum * sizeof (ptrlist->array[0]));
        }
        ptrlist->start = ptrlist->anum - ptrlist->num;
        memmove(ptrlist->array + ptrlist->start, ptrlist->array,
                ptrlist->num * sizeof (ptrlist->array[0]));
    }
    ptrlist->start--;
    ptrlist->array[ptrlist->start] = ptr;
    ptrlist->num++;
}

/* Remove an element of the top of a pointer list object. */
void *tcptrlistshift(TCPTRLIST *ptrlist) {
    assert(ptrlist);
    if (ptrlist->num < 1) return NULL;
    int index = ptrlist->start;
    ptrlist->start++;
    ptrlist->num--;
    void *rv = ptrlist->array[index];
    if ((ptrlist->start & 0xff) == 0 && ptrlist->start > (ptrlist->num >> 1)) {
        memmove(ptrlist->array, ptrlist->array + ptrlist->start,
                ptrlist->num * sizeof (ptrlist->array[0]));
        ptrlist->start = 0;
    }
    return rv;
}

/* Add an element at the specified location of a pointer list object. */
void tcptrlistinsert(TCPTRLIST *ptrlist, int index, void *ptr) {
    assert(ptrlist && index >= 0 && ptr);
    if (index > ptrlist->num) return;
    index += ptrlist->start;
    if (ptrlist->start + ptrlist->num >= ptrlist->anum) {
        ptrlist->anum += ptrlist->num + 1;
        TCREALLOC(ptrlist->array, ptrlist->array, ptrlist->anum * sizeof (ptrlist->array[0]));
    }
    memmove(ptrlist->array + index + 1, ptrlist->array + index,
            sizeof (ptrlist->array[0]) * (ptrlist->start + ptrlist->num - index));
    ptrlist->array[index] = ptr;
    ptrlist->num++;
}

/* Remove an element at the specified location of a pointer list object. */
void *tcptrlistremove(TCPTRLIST *ptrlist, int index) {
    assert(ptrlist && index >= 0);
    if (index >= ptrlist->num) return NULL;
    index += ptrlist->start;
    void *rv = ptrlist->array[index];
    ptrlist->num--;
    memmove(ptrlist->array + index, ptrlist->array + index + 1,
            sizeof (ptrlist->array[0]) * (ptrlist->start + ptrlist->num - index));
    return rv;
}

/* Overwrite an element at the specified location of a pointer list object. */
void tcptrlistover(TCPTRLIST *ptrlist, int index, void *ptr) {
    assert(ptrlist && index >= 0 && ptr);
    if (index >= ptrlist->num) return;
    index += ptrlist->start;
    ptrlist->array[index] = ptr;
}

/* Clear a pointer list object. */
void tcptrlistclear(TCPTRLIST *ptrlist) {
    assert(ptrlist);
    ptrlist->start = 0;
    ptrlist->num = 0;
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


#define TCBSENCUNIT    8192             // unit size of TCBS encoding
#define TCBWTCNTMIN    64               // minimum element number of counting sort
#define TCBWTCNTLV     4                // maximum recursion level of counting sort
#define TCBWTBUFNUM    16384            // number of elements of BWT buffer

typedef struct { // type of structure for a BWT character
    int fchr; // character code of the first character
    int tchr; // character code of the last character
} TCBWTREC;


/* private function prototypes */
static void tcglobalinit(void);
static void tcglobaldestroy(void);
static void tcbwtsortstrcount(const char **arrays, int anum, int len, int level);
static void tcbwtsortstrinsert(const char **arrays, int anum, int len, int skip);
static void tcbwtsortstrheap(const char **arrays, int anum, int len, int skip);
static void tcbwtsortchrcount(unsigned char *str, int len);
static void tcbwtsortchrinsert(unsigned char *str, int len);
static void tcbwtsortreccount(TCBWTREC *arrays, int anum);
static void tcbwtsortrecinsert(TCBWTREC *array, int anum);
static int tcbwtsearchrec(TCBWTREC *array, int anum, int tchr);
static void tcmtfencode(char *ptr, int size);
static void tcmtfdecode(char *ptr, int size);
static int tcgammaencode(const char *ptr, int size, char *obuf);
static int tcgammadecode(const char *ptr, int size, char *obuf);

int tcfilerrno2tcerr(int tcerrdef) {
#ifdef _WIN32
    switch (GetLastError()) {
        case ERROR_PATH_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
            return TCENOFILE;
        case ERROR_ACCESS_DENIED:
        case ERROR_WRITE_PROTECT:
        case ERROR_SHARING_VIOLATION:
            return TCENOPERM;
            break;
        case ERROR_SEEK:
            return TCESEEK;
            break;
        case ERROR_READ_FAULT:
            return TCEREAD;
            break;
        case ERROR_WRITE_FAULT:
            return TCEWRITE;
            break;
        case ERROR_LOCK_VIOLATION:
            return TCELOCK;
            break;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
            return TCEOPEN;
            break;
    }
#endif
    switch (errno) {
        case EACCES:
        case EROFS:
            return TCENOPERM;
            break;
        case ENOENT:
        case ENOTDIR:
            return TCENOFILE;
            break;
    }
    return tcerrdef;
}

/* Get the message string corresponding to an error code. */
const char *tcerrmsg(int ecode) {
    switch (ecode) {
        case TCESUCCESS: return "success";
        case TCETHREAD: return "threading error";
        case TCEINVALID: return "invalid operation";
        case TCENOFILE: return "file not found";
        case TCENOPERM: return "no permission";
        case TCEMETA: return "invalid meta data";
        case TCERHEAD: return "invalid record header";
        case TCEOPEN: return "open error";
        case TCECLOSE: return "close error";
        case TCETRUNC: return "trunc error";
        case TCESYNC: return "sync error";
        case TCESTAT: return "stat error";
        case TCESEEK: return "seek error";
        case TCEREAD: return "read error";
        case TCEWRITE: return "write error";
        case TCEMMAP: return "mmap error";
        case TCELOCK: return "lock error";
        case TCEUNLINK: return "unlink error";
        case TCERENAME: return "rename error";
        case TCEMKDIR: return "mkdir error";
        case TCERMDIR: return "rmdir error";
        case TCEKEEP: return "existing record";
        case TCENOREC: return "no record found";
        case TCETR: return "illegal transaction state";
        case TCEMISC: return "miscellaneous error";
        case TCEICOMPRESS: return "unsupported database compression format";
        case TCEDATACOMPRESS: return "data compression error";
    }
    return "unknown error";
}

/* Show error message on the standard error output and exit. */
void *tcmyfatal(const char *message) {
    assert(message);
    if (tcfatalfunc) {
        tcfatalfunc(message);
    } else {
        fprintf(stderr, "fatal error: %s\n", message);
    }
    exit(1);
    return NULL;
}

/* Allocate a large nullified region. */
void *tczeromap(uint64_t size) {
#if defined(_SYS_LINUX_)
    assert(size > 0);
    void *ptr = mmap(0, sizeof (size) + size,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) tcmyfatal("out of memory");
    *(uint64_t *) ptr = size;
    return (char *) ptr + sizeof (size);
#else
    assert(size > 0);
    void *ptr;
    TCCALLOC(ptr, 1, size);
    return ptr;
#endif
}

/* Free a large nullfied region. */
void tczerounmap(void *ptr) {
#if defined(_SYS_LINUX_)
    assert(ptr);
    uint64_t size = *((uint64_t *) ptr - 1);
    munmap((char *) ptr - sizeof (size), sizeof (size) + size);
#else
    assert(ptr);
    TCFREE(ptr);
#endif
}


/* Global mutex object. */
static pthread_once_t tcglobalonce = PTHREAD_ONCE_INIT;
static pthread_rwlock_t tcglobalmutex;
static pthread_mutex_t tcpathmutex;
static TCMAP *tcpathmap;

/* Lock the global mutex object. */
bool tcglobalmutexlock(void) {
    pthread_once(&tcglobalonce, tcglobalinit);
    return pthread_rwlock_wrlock(&tcglobalmutex) == 0;
}

/* Lock the global mutex object by shared locking. */
bool tcglobalmutexlockshared(void) {
    pthread_once(&tcglobalonce, tcglobalinit);
    return pthread_rwlock_rdlock(&tcglobalmutex) == 0;
}

/* Unlock the global mutex object. */
bool tcglobalmutexunlock(void) {
    return pthread_rwlock_unlock(&tcglobalmutex) == 0;
}

/* Lock the absolute path of a file. */
bool tcpathlock(const char *path) {
    assert(path);
    pthread_once(&tcglobalonce, tcglobalinit);
    if (pthread_mutex_lock(&tcpathmutex) != 0) return false;
    bool err = false;
    if (tcpathmap && !tcmapputkeep2(tcpathmap, path, "")) err = true;
    if (pthread_mutex_unlock(&tcpathmutex) != 0) err = true;
    return !err;
}

/* Unock the absolute path of a file. */
bool tcpathunlock(const char *path) {
    assert(path);
    pthread_once(&tcglobalonce, tcglobalinit);
    if (pthread_mutex_lock(&tcpathmutex) != 0) return false;
    bool err = false;
    if (tcpathmap && !tcmapout2(tcpathmap, path)) err = true;
    if (pthread_mutex_unlock(&tcpathmutex) != 0) err = true;
    return !err;
}

/* Convert an integer to the string as binary numbers. */
int tcnumtostrbin(uint64_t num, char *buf, int col, int fc) {
    assert(buf);
    char *wp = buf;
    int len = sizeof (num) * 8;
    bool zero = true;
    while (len-- > 0) {
        if (num & (1ULL << 63)) {
            *(wp++) = '1';
            zero = false;
        } else if (!zero) {
            *(wp++) = '0';
        }
        num <<= 1;
    }
    if (col > 0) {
        if (col > sizeof (num) * 8) col = sizeof (num) * 8;
        len = col - (wp - buf);
        if (len > 0) {
            memmove(buf + len, buf, wp - buf);
            for (int i = 0; i < len; i++) {
                buf[i] = fc;
            }
            wp += len;
        }
    } else if (zero) {
        *(wp++) = '0';
    }
    *wp = '\0';
    return wp - buf;
}

/* Compare keys of two records by lexical order. */
int tccmplexical(const char *aptr, int asiz, const char *bptr, int bsiz, void *op) {
    assert(aptr && asiz >= 0 && bptr && bsiz >= 0);
    int rv;
    TCCMPLEXICAL(rv, aptr, asiz, bptr, bsiz);
    return rv;
}

/* Compare two keys as decimal strings of real numbers. */
int tccmpdecimal(const char *aptr, int asiz, const char *bptr, int bsiz, void *op) {
    assert(aptr && asiz >= 0 && bptr && bsiz >= 0);
    const unsigned char *arp = (unsigned char *) aptr;
    int alen = asiz;
    while (alen > 0 && (*arp <= ' ' || *arp == 0x7f)) {
        arp++;
        alen--;
    }
    int64_t anum = 0;
    int asign = 1;
    if (alen > 0 && *arp == '-') {
        arp++;
        alen--;
        asign = -1;
    }
    while (alen > 0) {
        int c = *arp;
        if (c < '0' || c > '9') break;
        anum = anum * 10 + c - '0';
        arp++;
        alen--;
    }
    anum *= asign;
    const unsigned char *brp = (unsigned char *) bptr;
    int blen = bsiz;
    while (blen > 0 && (*brp <= ' ' || *brp == 0x7f)) {
        brp++;
        blen--;
    }
    int64_t bnum = 0;
    int bsign = 1;
    if (blen > 0 && *brp == '-') {
        brp++;
        blen--;
        bsign = -1;
    }
    while (blen > 0) {
        int c = *brp;
        if (c < '0' || c > '9') break;
        bnum = bnum * 10 + c - '0';
        brp++;
        blen--;
    }
    bnum *= bsign;
    if (anum < bnum) return -1;
    if (anum > bnum) return 1;
    if ((alen > 1 && *arp == '.') || (blen > 1 && *brp == '.')) {
        long double aflt = 0;
        if (alen > 1 && *arp == '.') {
            arp++;
            alen--;
            if (alen > TCLDBLCOLMAX) alen = TCLDBLCOLMAX;
            long double base = 10;
            while (alen > 0) {
                if (*arp < '0' || *arp > '9') break;
                aflt += (*arp - '0') / base;
                arp++;
                alen--;
                base *= 10;
            }
            aflt *= asign;
        }
        long double bflt = 0;
        if (blen > 1 && *brp == '.') {
            brp++;
            blen--;
            if (blen > TCLDBLCOLMAX) blen = TCLDBLCOLMAX;
            long double base = 10;
            while (blen > 0) {
                if (*brp < '0' || *brp > '9') break;
                bflt += (*brp - '0') / base;
                brp++;
                blen--;
                base *= 10;
            }
            bflt *= bsign;
        }
        if (aflt < bflt) return -1;
        if (aflt > bflt) return 1;
    }
    int rv;
    TCCMPLEXICAL(rv, aptr, asiz, bptr, bsiz);
    return rv;
}

/* Compare two keys as 32-bit integers in the native byte order. */
int tccmpint32(const char *aptr, int asiz, const char *bptr, int bsiz, void *op) {
    assert(aptr && bptr);
    int32_t anum, bnum;
    if (asiz == sizeof (int32_t)) {
        memcpy(&anum, aptr, sizeof (int32_t));
    } else if (asiz < sizeof (int32_t)) {
        memset(&anum, 0, sizeof (int32_t));
        memcpy(&anum, aptr, asiz);
    } else {
        memcpy(&anum, aptr, sizeof (int32_t));
    }
    if (bsiz == sizeof (int32_t)) {
        memcpy(&bnum, bptr, sizeof (int32_t));
    } else if (bsiz < sizeof (int32_t)) {
        memset(&bnum, 0, sizeof (int32_t));
        memcpy(&bnum, bptr, bsiz);
    } else {
        memcpy(&bnum, bptr, sizeof (int32_t));
    }
    return (anum < bnum) ? -1 : anum > bnum;
}

/* Compare two keys as 64-bit integers in the native byte order. */
int tccmpint64(const char *aptr, int asiz, const char *bptr, int bsiz, void *op) {
    assert(aptr && bptr);
    int64_t anum, bnum;
    if (asiz == sizeof (int64_t)) {
        memcpy(&anum, aptr, sizeof (int64_t));
    } else if (asiz < sizeof (int64_t)) {
        memset(&anum, 0, sizeof (int64_t));
        memcpy(&anum, aptr, asiz);
    } else {
        memcpy(&anum, aptr, sizeof (int64_t));
    }
    if (bsiz == sizeof (int64_t)) {
        memcpy(&bnum, bptr, sizeof (int64_t));
    } else if (bsiz < sizeof (int64_t)) {
        memset(&bnum, 0, sizeof (int64_t));
        memcpy(&bnum, bptr, bsiz);
    } else {
        memcpy(&bnum, bptr, sizeof (int64_t));
    }
    return (anum < bnum) ? -1 : anum > bnum;
}

/* Compress a serial object with TCBS encoding. */
char *tcbsencode(const char *ptr, int size, int *sp) {
    assert(ptr && size >= 0 && sp);
    char *result;
    TCMALLOC(result, (size * 7) / 3 + (size / TCBSENCUNIT + 1) * sizeof (uint16_t) +
            TCBSENCUNIT * 2 + 0x200);
    char *pv = result + size + 0x100;
    char *wp = pv;
    char *tp = pv + size + 0x100;
    const char *end = ptr + size;
    while (ptr < end) {
        int usiz = tclmin(TCBSENCUNIT, end - ptr);
        memcpy(tp, ptr, usiz);
        memcpy(tp + usiz, ptr, usiz);
        char *sp = wp;
        uint16_t idx = 0;
        wp += sizeof (idx);
        const char *arrays[usiz + 1];
        for (int i = 0; i < usiz; i++) {
            arrays[i] = tp + i;
        }
        const char *fp = arrays[0];
        if (usiz >= TCBWTCNTMIN) {
            tcbwtsortstrcount(arrays, usiz, usiz, 0);
        } else if (usiz > 1) {
            tcbwtsortstrinsert(arrays, usiz, usiz, 0);
        }
        for (int i = 0; i < usiz; i++) {
            int tidx = arrays[i] - fp;
            if (tidx == 0) {
                idx = i;
                *(wp++) = ptr[usiz - 1];
            } else {
                *(wp++) = ptr[tidx - 1];
            }
        }
        idx = TCHTOIS(idx);
        memcpy(sp, &idx, sizeof (idx));
        ptr += TCBSENCUNIT;
    }
    size = wp - pv;
    tcmtfencode(pv, size);
    int nsiz = tcgammaencode(pv, size, result);
    *sp = nsiz;
    return result;
}

/* Decompress a serial object compressed with TCBS encoding. */
char *tcbsdecode(const char *ptr, int size, int *sp) {
    char *result;
    TCMALLOC(result, size * 9 + 0x200);
    char *wp = result + size + 0x100;
    int nsiz = tcgammadecode(ptr, size, wp);
    tcmtfdecode(wp, nsiz);
    ptr = wp;
    wp = result;
    const char *end = ptr + nsiz;
    while (ptr < end) {
        uint16_t idx;
        memcpy(&idx, ptr, sizeof (idx));
        idx = TCITOHS(idx);
        ptr += sizeof (idx);
        int usiz = tclmin(TCBSENCUNIT, end - ptr);
        if (idx >= usiz) idx = 0;
        char rbuf[usiz + 1];
        memcpy(rbuf, ptr, usiz);
        if (usiz >= TCBWTCNTMIN) {
            tcbwtsortchrcount((unsigned char *) rbuf, usiz);
        } else if (usiz > 0) {
            tcbwtsortchrinsert((unsigned char *) rbuf, usiz);
        }
        int fnums[0x100], tnums[0x100];
        memset(fnums, 0, sizeof (fnums));
        memset(tnums, 0, sizeof (tnums));
        TCBWTREC array[usiz + 1];
        TCBWTREC *rp = array;
        for (int i = 0; i < usiz; i++) {
            int fc = *(unsigned char *) (rbuf + i);
            rp->fchr = (fc << 23) + fnums[fc]++;
            int tc = *(unsigned char *) (ptr + i);
            rp->tchr = (tc << 23) + tnums[tc]++;
            rp++;
        }
        unsigned int fchr = array[idx].fchr;
        if (usiz >= TCBWTCNTMIN) {
            tcbwtsortreccount(array, usiz);
        } else if (usiz > 1) {
            tcbwtsortrecinsert(array, usiz);
        }
        for (int i = 0; i < usiz; i++) {
            if (array[i].fchr == fchr) {
                idx = i;
                break;
            }
        }
        for (int i = 0; i < usiz; i++) {
            *(wp++) = array[idx].fchr >> 23;
            idx = tcbwtsearchrec(array, usiz, array[idx].fchr);
        }
        ptr += usiz;
    }
    *wp = '\0';
    *sp = wp - result;
    return result;
}

/* Encode a serial object with BWT encoding. */
char *tcbwtencode(const char *ptr, int size, int *idxp) {
    assert(ptr && size >= 0 && idxp);
    if (size < 1) {
        *idxp = 0;
        char *rv;
        TCMEMDUP(rv, "", 0);
        return rv;
    }
    char *result;
    TCMALLOC(result, size * 3 + 1);
    char *tp = result + size + 1;
    memcpy(tp, ptr, size);
    memcpy(tp + size, ptr, size);
    const char *abuf[TCBWTBUFNUM];
    const char **arrays = abuf;
    if (size > TCBWTBUFNUM) TCMALLOC(arrays, sizeof (*arrays) * size);
    for (int i = 0; i < size; i++) {
        arrays[i] = tp + i;
    }
    const char *fp = arrays[0];
    if (size >= TCBWTCNTMIN) {
        tcbwtsortstrcount(arrays, size, size, -1);
    } else if (size > 1) {
        tcbwtsortstrinsert(arrays, size, size, 0);
    }
    for (int i = 0; i < size; i++) {
        int idx = arrays[i] - fp;
        if (idx == 0) {
            *idxp = i;
            result[i] = ptr[size - 1];
        } else {
            result[i] = ptr[idx - 1];
        }
    }
    if (arrays != abuf) TCFREE(arrays);
    result[size] = '\0';
    return result;
}

/* Decode a serial object encoded with BWT encoding. */
char *tcbwtdecode(const char *ptr, int size, int idx) {
    assert(ptr && size >= 0);
    if (size < 1 || idx < 0) {
        char *rv;
        TCMEMDUP(rv, "", 0);
        return rv;
    }
    if (idx >= size) idx = 0;
    char *result;
    TCMALLOC(result, size + 1);
    memcpy(result, ptr, size);
    if (size >= TCBWTCNTMIN) {
        tcbwtsortchrcount((unsigned char *) result, size);
    } else {
        tcbwtsortchrinsert((unsigned char *) result, size);
    }
    int fnums[0x100], tnums[0x100];
    memset(fnums, 0, sizeof (fnums));
    memset(tnums, 0, sizeof (tnums));
    TCBWTREC abuf[TCBWTBUFNUM];
    TCBWTREC *array = abuf;
    if (size > TCBWTBUFNUM) TCMALLOC(array, sizeof (*array) * size);
    TCBWTREC *rp = array;
    for (int i = 0; i < size; i++) {
        int fc = *(unsigned char *) (result + i);
        rp->fchr = (fc << 23) + fnums[fc]++;
        int tc = *(unsigned char *) (ptr + i);
        rp->tchr = (tc << 23) + tnums[tc]++;
        rp++;
    }
    unsigned int fchr = array[idx].fchr;
    if (size >= TCBWTCNTMIN) {
        tcbwtsortreccount(array, size);
    } else if (size > 1) {
        tcbwtsortrecinsert(array, size);
    }
    for (int i = 0; i < size; i++) {
        if (array[i].fchr == fchr) {
            idx = i;
            break;
        }
    }
    char *wp = result;
    for (int i = 0; i < size; i++) {
        *(wp++) = array[idx].fchr >> 23;
        idx = tcbwtsearchrec(array, size, array[idx].fchr);
    }
    *wp = '\0';
    if (array != abuf) TCFREE(array);
    return result;
}

/* Get the binary logarithm of an integer. */
long tclog2l(long num) {
    if (num <= 1) return 0;
    num >>= 1;
    long rv = 0;
    while (num > 0) {
        rv++;
        num >>= 1;
    }
    return rv;
}

/* Get the binary logarithm of a real number. */
double tclog2d(double num) {
    return log(num) / log(2);
}

off_t tcpagsize(void) {
    static off_t g_pagesize = 0;
    if (!g_pagesize) {
#ifdef _WIN32
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        g_pagesize = system_info.dwPageSize;
#else
        g_pagesize = sysconf(_SC_PAGESIZE);
#endif
    }
    return g_pagesize;
}

/* Get the aligned offset of a file offset. */
uint64_t tcpagealign(uint64_t off) {
    off_t ps = tcpagsize();
    int diff = off & (ps - 1);
    return (diff > 0) ? (off + ps - diff) : off;
}

/* Initialize the global mutex object */
static void tcglobalinit(void) {
    if (pthread_rwlock_init(&tcglobalmutex, NULL) != 0) tcmyfatal("rwlock error");
    if (pthread_mutex_init(&tcpathmutex, NULL) != 0) tcmyfatal("mutex error");
    tcpathmap = tcmapnew2(TCMAPTINYBNUM);
    atexit(tcglobaldestroy);
}

/* Destroy the global mutex object */
static void tcglobaldestroy(void) {
    tcmapdel(tcpathmap);
    pthread_mutex_destroy(&tcpathmutex);
    pthread_rwlock_destroy(&tcglobalmutex);
}

/* Sort BWT string arrays by dicrionary order by counting sort.
   `array' specifies an array of string arrays.
   `anum' specifies the number of the array.
   `len' specifies the size of each string.
   `level' specifies the level of recursion. */
static void tcbwtsortstrcount(const char **arrays, int anum, int len, int level) {
    assert(arrays && anum >= 0 && len >= 0);
    const char *nbuf[TCBWTBUFNUM];
    const char **narrays = nbuf;
    if (anum > TCBWTBUFNUM) TCMALLOC(narrays, sizeof (*narrays) * anum);
    int count[0x100], accum[0x100];
    memset(count, 0, sizeof (count));
    int skip = level < 0 ? 0 : level;
    for (int i = 0; i < anum; i++) {
        count[((unsigned char *) arrays[i])[skip]]++;
    }
    memcpy(accum, count, sizeof (count));
    for (int i = 1; i < 0x100; i++) {
        accum[i] = accum[i - 1] + accum[i];
    }
    for (int i = 0; i < anum; i++) {
        narrays[--accum[((unsigned char *) arrays[i])[skip]]] = arrays[i];
    }
    int off = 0;
    if (level >= 0 && level < TCBWTCNTLV) {
        for (int i = 0; i < 0x100; i++) {
            int c = count[i];
            if (c > 1) {
                if (c >= TCBWTCNTMIN) {
                    tcbwtsortstrcount(narrays + off, c, len, level + 1);
                } else {
                    tcbwtsortstrinsert(narrays + off, c, len, skip + 1);
                }
            }
            off += c;
        }
    } else {
        for (int i = 0; i < 0x100; i++) {
            int c = count[i];
            if (c > 1) {
                if (c >= TCBWTCNTMIN) {
                    tcbwtsortstrheap(narrays + off, c, len, skip + 1);
                } else {
                    tcbwtsortstrinsert(narrays + off, c, len, skip + 1);
                }
            }
            off += c;
        }
    }
    memcpy(arrays, narrays, anum * sizeof (*narrays));
    if (narrays != nbuf) TCFREE(narrays);
}

/* Sort BWT string arrays by dicrionary order by insertion sort.
   `array' specifies an array of string arrays.
   `anum' specifies the number of the array.
   `len' specifies the size of each string.
   `skip' specifies the number of skipped bytes. */
static void tcbwtsortstrinsert(const char **arrays, int anum, int len, int skip) {
    assert(arrays && anum >= 0 && len >= 0);
    for (int i = 1; i < anum; i++) {
        int cmp = 0;
        const unsigned char *ap = (unsigned char *) arrays[i - 1];
        const unsigned char *bp = (unsigned char *) arrays[i];
        for (int j = skip; j < len; j++) {
            if (ap[j] != bp[j]) {
                cmp = ap[j] - bp[j];
                break;
            }
        }
        if (cmp > 0) {
            const char *swap = arrays[i];
            int j;
            for (j = i; j > 0; j--) {
                int cmp = 0;
                const unsigned char *ap = (unsigned char *) arrays[j - 1];
                const unsigned char *bp = (unsigned char *) swap;
                for (int k = skip; k < len; k++) {
                    if (ap[k] != bp[k]) {
                        cmp = ap[k] - bp[k];
                        break;
                    }
                }
                if (cmp < 0) break;
                arrays[j] = arrays[j - 1];
            }
            arrays[j] = swap;
        }
    }
}

/* Sort BWT string arrays by dicrionary order by heap sort.
   `array' specifies an array of string arrays.
   `anum' specifies the number of the array.
   `len' specifies the size of each string.
   `skip' specifies the number of skipped bytes. */
static void tcbwtsortstrheap(const char **arrays, int anum, int len, int skip) {
    assert(arrays && anum >= 0 && len >= 0);
    anum--;
    int bottom = (anum >> 1) + 1;
    int top = anum;
    while (bottom > 0) {
        bottom--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top) {
                int cmp = 0;
                const unsigned char *ap = (unsigned char *) arrays[i + 1];
                const unsigned char *bp = (unsigned char *) arrays[i];
                for (int j = skip; j < len; j++) {
                    if (ap[j] != bp[j]) {
                        cmp = ap[j] - bp[j];
                        break;
                    }
                }
                if (cmp > 0) i++;
            }
            int cmp = 0;
            const unsigned char *ap = (unsigned char *) arrays[mybot];
            const unsigned char *bp = (unsigned char *) arrays[i];
            for (int j = skip; j < len; j++) {
                if (ap[j] != bp[j]) {
                    cmp = ap[j] - bp[j];
                    break;
                }
            }
            if (cmp >= 0) break;
            const char *swap = arrays[mybot];
            arrays[mybot] = arrays[i];
            arrays[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
    while (top > 0) {
        const char *swap = arrays[0];
        arrays[0] = arrays[top];
        arrays[top] = swap;
        top--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top) {
                int cmp = 0;
                const unsigned char *ap = (unsigned char *) arrays[i + 1];
                const unsigned char *bp = (unsigned char *) arrays[i];
                for (int j = 0; j < len; j++) {
                    if (ap[j] != bp[j]) {
                        cmp = ap[j] - bp[j];
                        break;
                    }
                }
                if (cmp > 0) i++;
            }
            int cmp = 0;
            const unsigned char *ap = (unsigned char *) arrays[mybot];
            const unsigned char *bp = (unsigned char *) arrays[i];
            for (int j = 0; j < len; j++) {
                if (ap[j] != bp[j]) {
                    cmp = ap[j] - bp[j];
                    break;
                }
            }
            if (cmp >= 0) break;
            swap = arrays[mybot];
            arrays[mybot] = arrays[i];
            arrays[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
}

/* Sort BWT characters by code number by counting sort.
   `str' specifies a string.
   `len' specifies the length of the string. */
static void tcbwtsortchrcount(unsigned char *str, int len) {
    assert(str && len >= 0);
    int cnt[0x100];
    memset(cnt, 0, sizeof (cnt));
    for (int i = 0; i < len; i++) {
        cnt[str[i]]++;
    }
    int pos = 0;
    for (int i = 0; i < 0x100; i++) {
        memset(str + pos, i, cnt[i]);
        pos += cnt[i];
    }
}

/* Sort BWT characters by code number by insertion sort.
   `str' specifies a string.
   `len' specifies the length of the string. */
static void tcbwtsortchrinsert(unsigned char *str, int len) {
    assert(str && len >= 0);
    for (int i = 1; i < len; i++) {
        if (str[i - 1] - str[i] > 0) {
            unsigned char swap = str[i];
            int j;
            for (j = i; j > 0; j--) {
                if (str[j - 1] - swap < 0) break;
                str[j] = str[j - 1];
            }
            str[j] = swap;
        }
    }
}

/* Sort BWT records by code number by counting sort.
   `array' specifies an array of records.
   `anum' specifies the number of the array. */
static void tcbwtsortreccount(TCBWTREC *array, int anum) {
    assert(array && anum >= 0);
    TCBWTREC nbuf[TCBWTBUFNUM];
    TCBWTREC *narray = nbuf;
    if (anum > TCBWTBUFNUM) TCMALLOC(narray, sizeof (*narray) * anum);
    int count[0x100], accum[0x100];
    memset(count, 0, sizeof (count));
    for (int i = 0; i < anum; i++) {
        count[array[i].tchr >> 23]++;
    }
    memcpy(accum, count, sizeof (count));
    for (int i = 1; i < 0x100; i++) {
        accum[i] = accum[i - 1] + accum[i];
    }
    for (int i = 0; i < 0x100; i++) {
        accum[i] -= count[i];
    }
    for (int i = 0; i < anum; i++) {
        narray[accum[array[i].tchr >> 23]++] = array[i];
    }
    memcpy(array, narray, anum * sizeof (*narray));
    if (narray != nbuf) TCFREE(narray);
}

/* Sort BWT records by code number by insertion sort.
   `array' specifies an array of records..
   `anum' specifies the number of the array. */
static void tcbwtsortrecinsert(TCBWTREC *array, int anum) {
    assert(array && anum >= 0);
    for (int i = 1; i < anum; i++) {
        if (array[i - 1].tchr - array[i].tchr > 0) {
            TCBWTREC swap = array[i];
            int j;
            for (j = i; j > 0; j--) {
                if (array[j - 1].tchr - swap.tchr < 0) break;
                array[j] = array[j - 1];
            }
            array[j] = swap;
        }
    }
}

/* Search the element of BWT records.
   `array' specifies an array of records.
   `anum' specifies the number of the array.
   `tchr' specifies the last code number. */
static int tcbwtsearchrec(TCBWTREC *array, int anum, int tchr) {
    assert(array && anum >= 0);
    int bottom = 0;
    int top = anum;
    int mid;
    do {
        mid = (bottom + top) >> 1;
        if (array[mid].tchr == tchr) {
            return mid;
        } else if (array[mid].tchr < tchr) {
            bottom = mid + 1;
            if (bottom >= anum) break;
        } else {
            top = mid - 1;
        }
    } while (bottom <= top);
    return -1;
}


/* Initialization table for MTF encoder. */
const unsigned char tcmtftable[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/* Encode a region with MTF encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region. */
static void tcmtfencode(char *ptr, int size) {
    unsigned char table1[0x100], table2[0x100], *table, *another;
    assert(ptr && size >= 0);
    memcpy(table1, tcmtftable, sizeof (tcmtftable));
    table = table1;
    another = table2;
    const char *end = ptr + size;
    char *wp = ptr;
    while (ptr < end) {
        unsigned char c = *ptr;
        unsigned char *tp = table;
        unsigned char *tend = table + 0x100;
        while (tp < tend && *tp != c) {
            tp++;
        }
        int idx = tp - table;
        *(wp++) = idx;
        if (idx > 0) {
            memcpy(another, &c, 1);
            memcpy(another + 1, table, idx);
            memcpy(another + 1 + idx, table + idx + 1, 255 - idx);
            unsigned char *swap = table;
            table = another;
            another = swap;
        }
        ptr++;
    }
}

/* Decode a region compressed with MTF encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region. */
static void tcmtfdecode(char *ptr, int size) {
    assert(ptr && size >= 0);
    unsigned char table1[0x100], table2[0x100], *table, *another;
    assert(ptr && size >= 0);
    memcpy(table1, tcmtftable, sizeof (tcmtftable));
    table = table1;
    another = table2;
    const char *end = ptr + size;
    char *wp = ptr;
    while (ptr < end) {
        int idx = *(unsigned char *) ptr;
        unsigned char c = table[idx];
        *(wp++) = c;
        if (idx > 0) {
            memcpy(another, &c, 1);
            memcpy(another + 1, table, idx);
            memcpy(another + 1 + idx, table + idx + 1, 255 - idx);
            unsigned char *swap = table;
            table = another;
            another = swap;
        }
        ptr++;
    }
}

/* Encode a region with Elias gamma encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `obuf' specifies the pointer to the output buffer.
   The return value is the size of the output buffer. */
static int tcgammaencode(const char *ptr, int size, char *obuf) {
    assert(ptr && size >= 0 && obuf);
    TCBITSTRM strm;
    TCBITSTRMINITW(strm, obuf);
    const char *end = ptr + size;
    while (ptr < end) {
        unsigned int c = *(unsigned char *) ptr;
        if (!c) {
            TCBITSTRMCAT(strm, 1);
        } else {
            c++;
            int plen = 8;
            while (plen > 0 && !(c & (1 << plen))) {
                plen--;
            }
            int jlen = plen;
            while (jlen-- > 0) {
                TCBITSTRMCAT(strm, 0);
            }
            while (plen >= 0) {
                int sign = (c & (1 << plen)) > 0;
                TCBITSTRMCAT(strm, sign);
                plen--;
            }
        }
        ptr++;
    }
    TCBITSTRMSETEND(strm);
    return TCBITSTRMSIZE(strm);
}

/* Decode a region compressed with Elias gamma encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `obuf' specifies the pointer to the output buffer.
   The return value is the size of the output buffer. */
static int tcgammadecode(const char *ptr, int size, char *obuf) {
    assert(ptr && size >= 0 && obuf);
    char *wp = obuf;
    TCBITSTRM strm;
    TCBITSTRMINITR(strm, ptr, size);
    int bnum = TCBITSTRMNUM(strm);
    while (bnum > 0) {
        int sign;
        TCBITSTRMREAD(strm, sign);
        bnum--;
        if (sign) {
            *(wp++) = 0;
        } else {
            int plen = 1;
            while (bnum > 0) {
                TCBITSTRMREAD(strm, sign);
                bnum--;
                if (sign) break;
                plen++;
            }
            unsigned int c = 1;
            while (bnum > 0 && plen-- > 0) {
                TCBITSTRMREAD(strm, sign);
                bnum--;
                c = (c << 1) + (sign > 0);
            }
            *(wp++) = c - 1;
        }
    }
    return wp - obuf;
}

#include "utf8proc.h"

int tcicaseformat(const char *str, int strl, void *placeholder, int placeholdersz, char **dstptr) {
    if (strl <= 0 || *str == '\0') {
        if (placeholder && placeholdersz > 0) {
            *((char*) placeholder) = '\0';
            *dstptr = (char*) placeholder;
        } else {
            *dstptr = (char*) strdup("");
        }
        return 0;
    }
    return tcutf8map((const uint8_t*) str, strl, placeholder, placeholdersz, (uint8_t**) dstptr,
            UTF8PROC_COMPOSE | UTF8PROC_IGNORE | UTF8PROC_LUMP | UTF8PROC_CASEFOLD | UTF8PROC_STRIPMARK);
}

int tcutf8map(const uint8_t *str, int strl, void *placeholder, int placeholdersz, uint8_t **dstptr, int options) {
    int32_t *buffer;
    ssize_t result;
    *dstptr = NULL;
    result = utf8proc_decompose(str, strl, NULL, 0, options);
    if (result < 0) {
        return result;
    }
    if (!placeholder || placeholdersz < (result * sizeof (int32_t) + 1)) {
        buffer = malloc(result * sizeof (int32_t) + 1);
        if (!buffer) {
            return UTF8PROC_ERROR_NOMEM;
        }
    } else {
        buffer = placeholder;
    }
    result = utf8proc_decompose(str, strl, buffer, result, options);
    if (result < 0) {
        if (buffer != placeholder) {
            free(buffer);
        }
        return result;
    }
    result = utf8proc_reencode(buffer, result, options);
    if (result < 0) {
        if (buffer != placeholder) {
            free(buffer);
        }
        return result;
    }
    if (buffer != placeholder) {
        int32_t *newptr;
        newptr = realloc(buffer, (size_t) result + 1);
        if (newptr) {
            buffer = newptr;
        }
    }
    *dstptr = (uint8_t *) buffer;
    return result;
}

/**
 * Get the hash value by MurMur hashing.
 */
uint64_t hashmurmur64(const void* buf, size_t size, uint32_t seed) {
    assert(buf);
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ size;
    const uint64_t *data = (const uint64_t *) buf;
    const uint64_t *end = data + (size / 8);

    while (data != end) {
        uint64_t k = *data++;
        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }
    const unsigned char *data2 = (const unsigned char*) data;
    switch (size & 7) {
        case 7: h ^= (uint64_t) data2[6] << 48;
        case 6: h ^= (uint64_t) data2[5] << 40;
        case 5: h ^= (uint64_t) data2[4] << 32;
        case 4: h ^= (uint64_t) data2[3] << 24;
        case 3: h ^= (uint64_t) data2[2] << 16;
        case 2: h ^= (uint64_t) data2[1] << 8;
        case 1: h ^= (uint64_t) data2[0];
            h *= m;
    };
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

uint32_t hashmurmur32(const void * key, size_t len, uint32_t seed) {
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    uint32_t h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const unsigned char * data = (const unsigned char *) key;

    while (len >= 4) {
        uint32_t k = *(uint32_t*) data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch (len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
            h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}


// END OF FILE
