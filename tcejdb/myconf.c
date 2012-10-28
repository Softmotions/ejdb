/*************************************************************************************************
 * System-dependent configurations of Tokyo Cabinet
 *                                                               Copyright (C) 2006-2012 FAL Labs
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


#include "myconf.h"



/*************************************************************************************************
 * common settings
 *************************************************************************************************/


int _tc_dummy_cnt = 0;


int _tc_dummyfunc(void){
  return 0;
}


int _tc_dummyfuncv(int a, ...){
  return 0;
}



/*************************************************************************************************
 * for ZLIB
 *************************************************************************************************/


#if TCUSEZLIB


#include <zlib.h>

#define ZLIBBUFSIZ     8192


static char *_tc_deflate_impl(const char *ptr, int size, int *sp, int mode);
static char *_tc_inflate_impl(const char *ptr, int size, int *sp, int mode);
static unsigned int _tc_getcrc_impl(const char *ptr, int size);


char *(*_tc_deflate)(const char *, int, int *, int) = _tc_deflate_impl;
char *(*_tc_inflate)(const char *, int, int *, int) = _tc_inflate_impl;
unsigned int (*_tc_getcrc)(const char *, int) = _tc_getcrc_impl;


static char *_tc_deflate_impl(const char *ptr, int size, int *sp, int mode){
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  switch(mode){
    case _TCZMRAW:
      if(deflateInit2(&zs, 5, Z_DEFLATED, -15, 7, Z_DEFAULT_STRATEGY) != Z_OK)
        return NULL;
      break;
    case _TCZMGZIP:
      if(deflateInit2(&zs, 6, Z_DEFLATED, 15 + 16, 9, Z_DEFAULT_STRATEGY) != Z_OK)
        return NULL;
      break;
    default:
      if(deflateInit2(&zs, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return NULL;
      break;
  }
  int asiz = size + 16;
  if(asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  char *buf;
  if(!(buf = MYMALLOC(asiz))){
    deflateEnd(&zs);
    return NULL;
  }
  unsigned char obuf[ZLIBBUFSIZ];
  int bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  int rv;
  while((rv = deflate(&zs, Z_FINISH)) == Z_OK){
    int osiz = ZLIBBUFSIZ - zs.avail_out;
    if(bsiz + osiz > asiz){
      asiz = asiz * 2 + osiz;
      char *swap;
      if(!(swap = MYREALLOC(buf, asiz))){
        MYFREE(buf);
        deflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if(rv != Z_STREAM_END){
    MYFREE(buf);
    deflateEnd(&zs);
    return NULL;
  }
  int osiz = ZLIBBUFSIZ - zs.avail_out;
  if(bsiz + osiz + 1 > asiz){
    asiz = asiz * 2 + osiz;
    char *swap;
    if(!(swap = MYREALLOC(buf, asiz))){
      MYFREE(buf);
      deflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  if(mode == _TCZMRAW) bsiz++;
  *sp = bsiz;
  deflateEnd(&zs);
  return buf;
}


static char *_tc_inflate_impl(const char *ptr, int size, int *sp, int mode){
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  switch(mode){
    case _TCZMRAW:
      if(inflateInit2(&zs, -15) != Z_OK) return NULL;
      break;
    case _TCZMGZIP:
      if(inflateInit2(&zs, 15 + 16) != Z_OK) return NULL;
      break;
    default:
      if(inflateInit2(&zs, 15) != Z_OK) return NULL;
      break;
  }
  int asiz = size * 2 + 16;
  if(asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  char *buf;
  if(!(buf = MYMALLOC(asiz))){
    inflateEnd(&zs);
    return NULL;
  }
  unsigned char obuf[ZLIBBUFSIZ];
  int bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  int rv;
  while((rv = inflate(&zs, Z_NO_FLUSH)) == Z_OK){
    int osiz = ZLIBBUFSIZ - zs.avail_out;
    if(bsiz + osiz >= asiz){
      asiz = asiz * 2 + osiz;
      char *swap;
      if(!(swap = MYREALLOC(buf, asiz))){
        MYFREE(buf);
        inflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if(rv != Z_STREAM_END){
    MYFREE(buf);
    inflateEnd(&zs);
    return NULL;
  }
  int osiz = ZLIBBUFSIZ - zs.avail_out;
  if(bsiz + osiz >= asiz){
    asiz = asiz * 2 + osiz;
    char *swap;
    if(!(swap = MYREALLOC(buf, asiz))){
      MYFREE(buf);
      inflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  *sp = bsiz;
  inflateEnd(&zs);
  return buf;
}


static unsigned int _tc_getcrc_impl(const char *ptr, int size){
  assert(ptr && size >= 0);
  int crc = crc32(0, Z_NULL, 0);
  return crc32(crc, (unsigned char *)ptr, size);
}


#else


char *(*_tc_deflate)(const char *, int, int *, int) = NULL;
char *(*_tc_inflate)(const char *, int, int *, int) = NULL;
unsigned int (*_tc_getcrc)(const char *, int) = NULL;


#endif



/*************************************************************************************************
 * for BZIP2
 *************************************************************************************************/


#if TCUSEBZIP


#include <bzlib.h>

#define BZIPBUFSIZ     8192


static char *_tc_bzcompress_impl(const char *ptr, int size, int *sp);
static char *_tc_bzdecompress_impl(const char *ptr, int size, int *sp);


char *(*_tc_bzcompress)(const char *, int, int *) = _tc_bzcompress_impl;
char *(*_tc_bzdecompress)(const char *, int, int *) = _tc_bzdecompress_impl;


static char *_tc_bzcompress_impl(const char *ptr, int size, int *sp){
  assert(ptr && size >= 0 && sp);
  bz_stream zs;
  zs.bzalloc = NULL;
  zs.bzfree = NULL;
  zs.opaque = NULL;
  if(BZ2_bzCompressInit(&zs, 9, 0, 0) != BZ_OK) return NULL;
  int asiz = size + 16;
  if(asiz < BZIPBUFSIZ) asiz = BZIPBUFSIZ;
  char *buf;
  if(!(buf = MYMALLOC(asiz))){
    BZ2_bzCompressEnd(&zs);
    return NULL;
  }
  char obuf[BZIPBUFSIZ];
  int bsiz = 0;
  zs.next_in = (char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = BZIPBUFSIZ;
  int rv;
  while((rv = BZ2_bzCompress(&zs, BZ_FINISH)) == BZ_FINISH_OK){
    int osiz = BZIPBUFSIZ - zs.avail_out;
    if(bsiz + osiz > asiz){
      asiz = asiz * 2 + osiz;
      char *swap;
      if(!(swap = MYREALLOC(buf, asiz))){
        MYFREE(buf);
        BZ2_bzCompressEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = BZIPBUFSIZ;
  }
  if(rv != BZ_STREAM_END){
    MYFREE(buf);
    BZ2_bzCompressEnd(&zs);
    return NULL;
  }
  int osiz = BZIPBUFSIZ - zs.avail_out;
  if(bsiz + osiz + 1 > asiz){
    asiz = asiz * 2 + osiz;
    char *swap;
    if(!(swap = MYREALLOC(buf, asiz))){
      MYFREE(buf);
      BZ2_bzCompressEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  *sp = bsiz;
  BZ2_bzCompressEnd(&zs);
  return buf;
}


static char *_tc_bzdecompress_impl(const char *ptr, int size, int *sp){
  assert(ptr && size >= 0 && sp);
  bz_stream zs;
  zs.bzalloc = NULL;
  zs.bzfree = NULL;
  zs.opaque = NULL;
  if(BZ2_bzDecompressInit(&zs, 0, 0) != BZ_OK) return NULL;
  int asiz = size * 2 + 16;
  if(asiz < BZIPBUFSIZ) asiz = BZIPBUFSIZ;
  char *buf;
  if(!(buf = MYMALLOC(asiz))){
    BZ2_bzDecompressEnd(&zs);
    return NULL;
  }
  char obuf[BZIPBUFSIZ];
  int bsiz = 0;
  zs.next_in = (char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = BZIPBUFSIZ;
  int rv;
  while((rv = BZ2_bzDecompress(&zs)) == BZ_OK){
    int osiz = BZIPBUFSIZ - zs.avail_out;
    if(bsiz + osiz >= asiz){
      asiz = asiz * 2 + osiz;
      char *swap;
      if(!(swap = MYREALLOC(buf, asiz))){
        MYFREE(buf);
        BZ2_bzDecompressEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = BZIPBUFSIZ;
  }
  if(rv != BZ_STREAM_END){
    MYFREE(buf);
    BZ2_bzDecompressEnd(&zs);
    return NULL;
  }
  int osiz = BZIPBUFSIZ - zs.avail_out;
  if(bsiz + osiz >= asiz){
    asiz = asiz * 2 + osiz;
    char *swap;
    if(!(swap = MYREALLOC(buf, asiz))){
      MYFREE(buf);
      BZ2_bzDecompressEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  *sp = bsiz;
  BZ2_bzDecompressEnd(&zs);
  return buf;
}


#else


char *(*_tc_bzcompress)(const char *, int, int *) = NULL;
char *(*_tc_bzdecompress)(const char *, int, int *) = NULL;


#endif



/*************************************************************************************************
 * for test of custom codec functions
 *************************************************************************************************/


#if TCUSEEXLZMA


#include <lzmalib.h>


void *_tc_recencode(const void *ptr, int size, int *sp, void *op){
  return lzma_compress(ptr, size, sp);
}


void *_tc_recdecode(const void *ptr, int size, int *sp, void *op){
  return lzma_decompress(ptr, size, sp);
}


#elif TCUSEEXLZO


#include <lzo/lzo1x.h>


bool _tc_lzo_init = false;


void *_tc_recencode(const void *ptr, int size, int *sp, void *op){
  if(!_tc_lzo_init){
    if(lzo_init() != LZO_E_OK) return NULL;
    _tc_lzo_init = false;
  }
  lzo_bytep buf = MYMALLOC(size + (size >> 4) + 80);
  if(!buf) return NULL;
  lzo_uint bsiz;
  char wrkmem[LZO1X_1_MEM_COMPRESS];
  if(lzo1x_1_compress((lzo_bytep)ptr, size, buf, &bsiz, wrkmem) != LZO_E_OK){
    MYFREE(buf);
    return NULL;
  }
  buf[bsiz] = '\0';
  *sp = bsiz;
  return (char *)buf;
}


void *_tc_recdecode(const void *ptr, int size, int *sp, void *op){
  if(!_tc_lzo_init){
    if(lzo_init() != LZO_E_OK) return NULL;
    _tc_lzo_init = false;
  }
  lzo_bytep buf;
  lzo_uint bsiz;
  int rat = 6;
  while(true){
    bsiz = (size + 256) * rat + 3;
    buf = MYMALLOC(bsiz + 1);
    if(!buf) return NULL;
    int rv = lzo1x_decompress_safe((lzo_bytep)ptr, size, buf, &bsiz, NULL);
    if(rv == LZO_E_OK){
      break;
    } else if(rv == LZO_E_OUTPUT_OVERRUN){
      MYFREE(buf);
      rat *= 2;
    } else {
      MYFREE(buf);
      return NULL;
    }
  }
  buf[bsiz] = '\0';
  if(sp) *sp = bsiz;
  return (char *)buf;
}


#else


void *_tc_recencode(const void *ptr, int size, int *sp, void *op){
  char *res = MYMALLOC(size + 1);
  if(!res) return NULL;
  memcpy(res, ptr, size);
  *sp = size;
  return res;
}


void *_tc_recdecode(const void *ptr, int size, int *sp, void *op){
  char *res = MYMALLOC(size + 1);
  if(!res) return NULL;
  memcpy(res, ptr, size);
  *sp = size;
  return res;
}


#endif



// END OF FILE
