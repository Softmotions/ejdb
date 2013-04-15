/*
 * File:   basic.h
 * Author: adam
 *
 * Created on April 1, 2013, 6:21 PM
 */

#ifndef BASIC_H
#define	BASIC_H

#ifdef __cplusplus
#define EJDB_EXTERN_C_START extern "C" {
#define EJDB_EXTERN_C_END }
#else
#define EJDB_EXTERN_C_START
#define EJDB_EXTERN_C_END
#endif

EJDB_EXTERN_C_START

#ifdef __GNUC__
#define EJDB_INLINE static inline
#else
#define EJDB_INLINE static
#endif

#ifdef _WIN32
#ifdef EJDB_DLL
#define EJDB_EXPORT __declspec(dllexport)
#elif !defined(EJDB_STATIC)
#define EJDB_EXPORT __declspec(dllimport)
#else
#define EJDB_EXPORT
#endif
#else
#define EJDB_EXPORT
#endif

#ifdef _WIN32
#include <windows.h>
#define INVALIDHANDLE(_HNDL) (((_HNDL) == INVALID_HANDLE_VALUE) || (_HNDL) == NULL)
#else
typedef int HANDLE;
#define INVALID_HANDLE_VALUE (-1)
#define INVALIDHANDLE(_HNDL) ((_HNDL) < 0 || (_HNDL) == UINT16_MAX)
#endif


#define JDBIDKEYNAME "_id"  /**> Name of PK _id field in BSONs */
#define JDBIDKEYNAMEL 3

EJDB_EXTERN_C_END
#endif	/* BASIC_H */

