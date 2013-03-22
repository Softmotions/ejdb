/**************************************************************************************************
 *  Ruby API for EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
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

#include <tcejdb/ejdb_private.h>
#include <ruby.h>

typedef struct {
    EJDB* ejdb;
} RUBEJDB;

EJDB* getEJDB(VALUE self) {
    RUBEJDB* rejdb;
    Data_Get_Struct(self, RUBEJDB, rejdb);
    return rejdb->ejdb;
}

void EJDB_free(RUBEJDB* rejdb) {
    if (!rejdb->ejdb) {
        ejdbdel(rejdb->ejdb);
    }
    ruby_xfree(rejdb);
}

VALUE EJDB_alloc(VALUE klass) {
    return Data_Wrap_Struct(klass, NULL, EJDB_free, ruby_xmalloc(sizeof(RUBEJDB)));
}

VALUE EJDB_init(VALUE self) {
    RUBEJDB* rejdb;
    Data_Get_Struct(self, RUBEJDB, rejdb);

    rejdb->ejdb = ejdbnew();
    if (!rejdb->ejdb) {
        rb_raise(rb_eRuntimeError, "Failed to init ejdb!");
    }
}

VALUE EJDB_open(VALUE self, VALUE path, VALUE mode) {
    Check_Type(path, T_STRING);
    Check_Type(mode, T_FIXNUM);
    EJDB* ejdb = getEJDB(self);
    if (!ejdbopen(ejdb, StringValuePtr(path), FIX2INT(mode))) {
        return set_ejdb_error(ejdb);
    }
    return Qnil;
}

VALUE EJDB_is_open(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    return ejdb && ejdbisopen(ejdb) ? Qtrue : Qfalse;
}


Init_rubejdb() {
    VALUE ejdbClass = rb_define_class("EJDB", rb_cObject);
    rb_define_alloc_func(ejdbClass, EJDB_alloc);
    rb_define_private_method(ejdbClass, "initialize", RUBY_METHOD_FUNC(EJDB_init), 0);
    rb_define_method(ejdbClass, "open", RUBY_METHOD_FUNC(EJDB_open), 2);
    rb_define_method(ejdbClass, "is_open", RUBY_METHOD_FUNC(EJDB_is_open), 0);
}