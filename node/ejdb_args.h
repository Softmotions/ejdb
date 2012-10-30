/*
 * File:   node_args.h
 * Author: adam
 *
 * Created on October 30, 2012, 10:37 AM
 */
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef EJDB_ARGS_H
#define	EJDB_ARGS_H

#include <v8.h>
#include <node.h>


#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    return ThrowException(Exception::TypeError(                         \
                             String::New("Expected " #N " arguments")));

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a string")));    \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_STRW_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a string")));    \
  String::Value VAR(args[I]->ToString());


#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a function")));  \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_OBJ_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsObject())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a object")));    \
  Local<Object> VAR = Local<Object>::Cast(args[I]);

#define REQ_VAL_ARG(I, VAR)                                             \
  if (args.Length() <= (I))												\
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a provided")));  \
  Local<Value> VAR = args[I];


#define REQ_ARR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsArray())                      \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an array")));    \
  Local<Array> VAR = Local<Array>::Cast(args[I]);


#define REQ_EXT_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())                   \
    return ThrowException(Exception::TypeError(                         \
                              String::New("Argument " #I " invalid"))); \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define OPT_INT_ARG(I, VAR, DEFAULT)                                    \
  int VAR;                                                              \
  if (args.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
  } else if (args[I]->IsInt32()) {                                      \
    VAR = args[I]->Int32Value();                                        \
  } else {                                                              \
    return ThrowException(Exception::TypeError(                         \
              String::New("Argument " #I " must be an integer")));      \
  }

#define REQ_INT32_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsInt32())                      \
    return ThrowException(Exception::TypeError(                         \
               String::New("Argument " #I " must be an integer")));     \
  int32_t VAR = args[I]->Int32Value();


#define REQ_INT64_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsNumber())                      \
    return ThrowException(Exception::TypeError(                         \
               String::New("Argument " #I " must be an number")));     \
  int64_t VAR = args[I]->IntegerValue();



#define REQ_NUM_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsNumber())                     \
    return ThrowException(Exception::TypeError(                         \
               String::New("Argument " #I " must be an number")));      \
  double VAR = args[I]->ToNumber()->Value();


#define REQ_BUFF_ARG(I, VAR)                                            \
  if (!Buffer::HasInstance(args[I])) {                                  \
     return ThrowException(Exception::Error(                            \
                String::New("Argument " #I " must to be a buffer")));   \
  }                                                                     \
  Buffer* VAR = ObjectWrap::Unwrap<Buffer>(args[I]->ToObject());


#endif	/* EJDB_ARGS_H */

