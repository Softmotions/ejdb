
#include "luabson.h"
#include <lualib.h>
#include <lauxlib.h>

static void lua_push_bson_value(lua_State *L, bson_iterator *it);
static void lua_push_bson_table(lua_State *L, bson_iterator *it);
static void lua_push_bson_array(lua_State *L, bson_iterator *it);

void lua_init_bson(lua_State *L) {
    if (!lua_istable(L, -1)) {
        return luaL_error(L, "luainitbson: Table must be on top of lua stack");
    }
    lua_pushcfunction(L, lua_from_bson);
    lua_setfield(L, -2, "lua_from_bson");

    lua_pushcfunction(L, lua_to_bson);
    lua_setfield(L, -2, "lua_to_bson");
}

static int null_value(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

void lua_push_bsontype_table(lua_State* L, int bsontype) {
    lua_newtable(L); //table
    lua_newtable(L); //metatable
    lua_pushstring(L, "__bsontype");
    lua_pushinteger(L, bsontype);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
}

static void lua_push_bson_value(lua_State *L, bson_iterator *it) {
    bson_type bt = bson_iterator_type(it);
    switch (bt) {
        case BSON_OID:
        {
            char xoid[25];
            bson_oid_to_string(bson_iterator_oid(it), xoid);
            lua_pushstring(L, xoid);
            break;
        }
        case BSON_STRING:
        case BSON_SYMBOL:
            lua_pushstring(L, bson_iterator_string(it));
            break;
        case BSON_NULL:
        case BSON_UNDEFINED:
            lua_pushnil(L);
            break;
        case BSON_INT:
            lua_pushinteger(L, bson_iterator_int(it));
            break;
        case BSON_LONG:
        case BSON_DOUBLE:
            lua_pushnumber(L, (lua_Number) bson_iterator_double(it));
            break;
        case BSON_BOOL:
            lua_pushboolean(L, bson_iterator_bool(it));
            break;
        case BSON_OBJECT:
        case BSON_ARRAY:
        {
            bson_iterator nit;
            bson_iterator_subiterator(it, &nit);
            if (bt == BSON_OBJECT) {
                lua_push_bson_table(L, &nit);
            } else {
                lua_push_bson_array(L, &nit);
            }
            break;
        }
        case BSON_DATE:
        {
            lua_push_bsontype_table(L, bt);
            lua_pushnumber(L, bson_iterator_date(it));
            lua_rawseti(L, -2, 1);
            break;
        }
        case BSON_BINDATA:
        {
            lua_push_bsontype_table(L, bt);
            lua_pushlstring(L, bson_iterator_bin_data(it), bson_iterator_bin_len(it));
            break;
        }
        case BSON_REGEX:
        {
            const char *re = bson_iterator_regex(it);
            const char *ro = bson_iterator_regex_opts(it);
            lua_push_bsontype_table(L, bt);
            lua_pushstring(L, re);
            lua_rawseti(L, -2, 1);
            lua_pushstring(L, ro);
            lua_rawseti(L, -2, 2);
            break;
        }
        default:
            break;

    }
}

static void lua_push_bson_table(lua_State *L, bson_iterator *it) {
    bson_type bt;
    lua_push_bsontype_table(L, BSON_OBJECT);
    while ((bt = bson_iterator_next(it)) != BSON_EOO) {
        lua_pushstring(L, bson_iterator_key(it));
        lua_push_bson_value(L, it);
        lua_rawset(L, -3);
    }
}

static void lua_push_bson_array(lua_State *L, bson_iterator *it) {
    bson_type bt;
    lua_push_bsontype_table(L, BSON_ARRAY);
    for (int i = 1; (bt = bson_iterator_next(it)) != BSON_EOO; ++i) {
        lua_push_bson_value(L, it);
        lua_rawseti(L, -2, i);
    }
}

int lua_from_bson(lua_State *L) {
    const void *bsdata = lua_tostring(L, -1);
    if (!bsdata) {
        return luaL_error(L, "luafrombson: BSON binary string must be on top of stack");
    }
    bson_iterator it;
    bson_iterator_from_buffer(&it, bsdata);
    lua_push_bson_table(L, &it);
    return 0;
}

int lua_to_bson(lua_State *L) {
    return 0;
}


