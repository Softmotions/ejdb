/*
 * File:   luabson.h
 * Author: adam
 *
 * Created on March 10, 2013, 5:11 PM
 */

#ifndef LUABSON_H
#define	LUABSON_H

#include <lua.h>
#include <tcejdb/bson.h>

#ifdef	__cplusplus
extern "C" {
#endif

    void lua_init_bson(lua_State *L);
    int lua_from_bson(lua_State *L);
    int lua_to_bson(lua_State *L);
    int print_bson(lua_State *L);
    int check_valid_oid_string(lua_State *L);
    void lua_push_bsontype_table(lua_State* L, int bsontype);
    void lua_push_bson_table(lua_State *L, bson_iterator *it);

#ifdef	__cplusplus
}
#endif

#endif	/* LUABSON_H */

