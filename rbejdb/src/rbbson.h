/*
 * File:   rbbson.h
 * Author: yudanov
 *
 * Created on March 25, 2013, 2:31 PM
 */

#ifndef RBBSON_H
#define	RBBSON_H

#include <ruby.h>
#include <tcejdb/bson.h>

#define RUBY_TO_BSON_AS_QUERY 1

#ifdef	__cplusplus
extern "C" {
#endif

    void init_ruby_to_bson();

    void ruby_to_bson(VALUE rbobj, bson** bsonbuf, int flags);

    VALUE bson_to_ruby(bson* bsonval);

#ifdef	__cplusplus
}
#endif

#endif	/* RBBSON_H */

