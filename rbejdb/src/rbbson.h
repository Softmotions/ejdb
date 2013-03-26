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

#ifdef	__cplusplus
extern "C" {
#endif

    void ruby_to_bson(VALUE rbobj, bson** bsonbuf);

    VALUE bson_to_ruby(bson* bsonval);

#ifdef	__cplusplus
}
#endif

#endif	/* RBBSON_H */

