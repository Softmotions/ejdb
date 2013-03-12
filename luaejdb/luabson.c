
#include "luabson.h"
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>

static void lua_push_bson_value(lua_State *L, bson_iterator *it);
static void lua_push_bson_table(lua_State *L, bson_iterator *it);
static void lua_push_bson_array(lua_State *L, bson_iterator *it);
static void lua_to_bson_impl(lua_State *L, int spos, bson *bs);

void lua_init_bson(lua_State *L) {
    if (!lua_istable(L, -1)) {
        luaL_error(L, "luainitbson: Table must be on top of lua stack");
        return;
    }
    lua_pushcfunction(L, lua_from_bson);
    lua_setfield(L, -2, "from_bson");

    lua_pushcfunction(L, lua_to_bson);
    lua_setfield(L, -2, "to_bson");

    lua_pushcfunction(L, lua_query_to_bson);
    lua_setfield(L, -2, "query_to_bson");

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
    const void *bsdata = luaL_checkstring(L, lua_gettop(L));
    bson_iterator it;
    bson_iterator_from_buffer(&it, bsdata);
    lua_push_bson_table(L, &it);
    return 1;
}

int lua_to_bson(lua_State *L) {
    luaL_checktype(L, lua_gettop(L), LUA_TTABLE);
    bson bs;
    bson_init_as_query(&bs);
    lua_to_bson_impl(L, lua_gettop(L), &bs);
    if (bs.err || bson_finish(&bs)) {
        lua_pushstring(L, bson_first_errormsg(&bs));
        bson_destroy(&bs);
        return lua_error(L);
    }
    lua_pushlstring(L, bson_data(&bs), bson_size(&bs));
    bson_destroy(&bs);
    return 1;
}

static void lua_val_to_bson(lua_State *L, const char *key, int vpos, bson *bs, int tref) {
    int vtype = lua_type(L, vpos);
    char nbuf[TCNUMBUFSIZ];
    switch (vtype) {
        case LUA_TTABLE:
        {
            if (vpos < 0) vpos = lua_gettop(L) + vpos + 1;
            lua_checkstack(L, 3);
            int bsontype_found = luaL_getmetafield(L, vpos, "__bsontype");
            if (!bsontype_found) {
                //check if registry tbl contains traversed tbl
                lua_rawgeti(L, LUA_REGISTRYINDEX, tref);
                lua_pushvalue(L, vpos);
                lua_rawget(L, -2);
                if (lua_toboolean(L, -1)) { //already traversed
                    lua_pop(L, 2);
                    break;
                }
                //setup traversed state
                lua_pop(L, 1);
                lua_pushvalue(L, vpos);
                lua_pushboolean(L, 1);
                lua_rawset(L, -3);
                lua_pop(L, 1);

                bool array = true;
                int len = 0;
                for (lua_pushnil(L); lua_next(L, vpos); lua_pop(L, 1)) {
                    ++len;
                    if ((lua_type(L, -2) != LUA_TNUMBER) || (lua_tointeger(L, -2) != len)) {
                        lua_pop(L, 2);
                        array = false;
                        break;
                    }
                }
                if (array) {
                    bson_append_start_array(bs, key);
                    for (int i = 0; i < len; i++) {
                        lua_rawgeti(L, vpos, i + 1);
                        bson_numstrn(nbuf, TCNUMBUFSIZ, (int64_t) i);
                        lua_val_to_bson(L, nbuf, -1, bs, tref);
                        lua_pop(L, 1);
                    }
                    bson_append_finish_array(bs);
                } else {
                    for (lua_pushnil(L); lua_next(L, vpos); lua_pop(L, 1)) {
                        int ktype = lua_type(L, -2);
                        if (ktype == LUA_TNUMBER) {
                            char key[TCNUMBUFSIZ];
                            bson_numstrn(key, TCNUMBUFSIZ, (int64_t) lua_tointeger(L, -2));
                            lua_val_to_bson(L, key, -1, bs, tref);
                        } else if (ktype == LUA_TSTRING) {
                            lua_val_to_bson(L, lua_tostring(L, -2), -1, bs, tref);
                        }
                    }
                }
            } else {
                int bson_type = lua_tointeger(L, -1);
                lua_pop(L, 1); //pop metafield __bsontype
                lua_rawgeti(L, -1, 1); //get first value
                switch (bson_type) {
                    case BSON_OID:
                    {
                        const char* boid = lua_tostring(L, -1);
                        if (boid && strlen(boid) == 24) {
                            bson_oid_t oid;
                            bson_oid_from_string(&oid, boid);
                            bson_append_oid(bs, key, &oid);
                        }
                        break;
                    }
                    case BSON_DATE:
                        bson_append_date(bs, key, (bson_date_t) lua_tonumber(L, -1));
                        break;
                    case BSON_REGEX:
                    {
                        const char* regex = lua_tostring(L, -1);
                        lua_rawgeti(L, -2, 2); // re opts
                        const char* options = lua_tostring(L, -1);
                        lua_pop(L, 1);
                        if (regex && options) {
                            bson_append_regex(bs, key, regex, options);
                        }
                        break;
                    }
                    case BSON_BINDATA:
                    {
                        size_t len;
                        const char* cbuf = lua_tolstring(L, -1, &len);
                        bson_append_binary(bs, key, BSON_BIN_BINARY, cbuf, len);
                        break;
                    }
                    case BSON_NULL:
                        bson_append_null(bs, key);
                        break;
                    case BSON_UNDEFINED:
                        bson_append_undefined(bs, key);
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case LUA_TNIL:
            bson_append_null(bs, key);
            break;
        case LUA_TNUMBER:
        {
            lua_Number numval = lua_tonumber(L, vpos);
            if (numval == floor(numval)) {
                int64_t iv = (int64_t) numval;
                if (-(1LL << 31) <= iv && iv <= (1LL << 31)) {
                    bson_append_int(bs, key, iv);
                } else {
                    bson_append_long(bs, key, iv);
                }
            }
            break;
        }
        case LUA_TBOOLEAN:
            bson_append_bool(bs, key, lua_toboolean(L, vpos));
            break;

        case LUA_TSTRING:
            bson_append_string(bs, key, lua_tostring(L, vpos));
            break;
    }
}

static void lua_to_bson_impl(lua_State *L, int tpos, bson *bs) {
    lua_newtable(L); //traverse state table
    int tref = luaL_ref(L, LUA_REGISTRYINDEX);
    for (lua_pushnil(L); lua_next(L, tpos); lua_pop(L, 1)) {
        int ktype = lua_type(L, -2);
        if (ktype == LUA_TNUMBER) {
            char key[TCNUMBUFSIZ];
            bson_numstrn(key, TCNUMBUFSIZ, (int64_t) lua_tonumber(L, -2));
            lua_val_to_bson(L, key, -1, bs, tref);
        } else if (ktype == LUA_TSTRING) {
            const char* key = lua_tostring(L, -2);
            if (lua_type(L, -1) == LUA_TSTRING && !strcmp("_id", key)) { //root level OID as string
                //pack OID as type table
                lua_pushvalue(L, -1); //dup oid on stack
                lua_push_bsontype_table(L, BSON_OID); //push type table
                lua_rawseti(L, -2, 1); //pop oid val
            }
            lua_val_to_bson(L, key, -1, bs, tref);
        }
    }
    lua_unref(L, tref);
}

int lua_query_to_bson(lua_State *L) {
    //todo
    return 0;
}

static void lua_query_to_bson_impl(lua_State *L, int tpos, bson *bs) {
    //todo
}


