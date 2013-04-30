
#include "luabson.h"
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <string.h>
#include <tcejdb/ejdb.h>
#include <tcejdb/myconf.h>

static void lua_to_bson_impl(lua_State *L, int spos, bson *bs);
static void bson_print_xstr(TCXSTR* xstr, const char *data, int depth);

void lua_init_bson(lua_State *L) {
    if (!lua_istable(L, -1)) {
        luaL_error(L, "luainitbson: Table must be on top of lua stack");
        return;
    }
    lua_pushcfunction(L, lua_from_bson);
    lua_setfield(L, -2, "from_bson");

    lua_pushcfunction(L, lua_to_bson);
    lua_setfield(L, -2, "to_bson");

    lua_pushcfunction(L, print_bson);
    lua_setfield(L, -2, "print_bson");

    lua_pushcfunction(L, check_valid_oid_string);
    lua_setfield(L, -2, "check_valid_oid_string");
}

int check_valid_oid_string(lua_State *L) {
    bool ret = false;
    if (lua_type(L, 1) == LUA_TSTRING) {
        ret = ejdbisvalidoidstr(lua_tostring(L, -1));
    }
    if (!ret) {
        return luaL_error(L, "OID is not valid");
    }
    return 0;
}

int print_bson(lua_State *L) {
    lua_settop(L, 1);
    size_t slen = 0;
    const void *bsdata = luaL_checklstring(L, lua_gettop(L), &slen);
    if (slen <= 4 || !bsdata) {
        return luaL_error(L, "Invalid bson string at argument #1");
    }
    TCXSTR *xstr = tcxstrnew();
    bson_print_xstr(xstr, bsdata, 0);
    lua_pushstring(L, TCXSTRPTR(xstr));
    tcxstrdel(xstr);
    return 1;
}

//-0/+1

void lua_push_bsontype_table(lua_State* L, int bsontype) {
    lua_newtable(L); //table
    lua_newtable(L); //metatable
    lua_pushinteger(L, bsontype);
    lua_setfield(L, -2, "__bsontype");
    lua_setmetatable(L, -2);
}

void lua_push_bson_value(lua_State *L, bson_iterator *it) {
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
            lua_push_bsontype_table(L, bt);
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

void lua_push_bson_table(lua_State *L, bson_iterator *it) {
    bson_type bt;
    lua_push_bsontype_table(L, BSON_OBJECT);
    while ((bt = bson_iterator_next(it)) != BSON_EOO) {
        lua_pushstring(L, bson_iterator_key(it));
        lua_push_bson_value(L, it);
        lua_rawset(L, -3);
    }
}

void lua_push_bson_array(lua_State *L, bson_iterator *it) {
    bson_type bt;
    lua_push_bsontype_table(L, BSON_ARRAY);
    int i;
    for (i = 1; (bt = bson_iterator_next(it)) != BSON_EOO; ++i) {
        lua_push_bson_value(L, it);
        lua_rawseti(L, -2, i);
    }
}

int lua_from_bson(lua_State *L) {
    lua_settop(L, 1);
    size_t slen = 0;
    const void *bsdata = luaL_checklstring(L, lua_gettop(L), &slen);
    if (slen <= 4 || !bsdata) {
        return luaL_error(L, "Invalid bson string at argument #1");
    }
    bson_iterator it;
    bson_iterator_from_buffer(&it, bsdata);
    lua_push_bson_table(L, &it);
    return 1;
}

int lua_to_bson(lua_State *L) {
    lua_settop(L, 1);
    int argc = lua_gettop(L);
    luaL_checktype(L, lua_gettop(L), LUA_TTABLE);
    bson bs;
    bson_init_as_query(&bs);
    lua_to_bson_impl(L, lua_gettop(L), &bs);
    if (bs.err || bson_finish(&bs)) {
        lua_pushstring(L, bson_first_errormsg(&bs));
        bson_destroy(&bs);
        return lua_error(L);
    }
    lua_pushlstring(L, bson_data(&bs), bson_size(&bs)); //+1 bson data as string
    bson_destroy(&bs);
    if (lua_gettop(L) - argc != 1) {
        return luaL_error(L, "lua_to_bson: Invalid stack size: %d should be: %d", (lua_gettop(L) - argc), 1);
    }
    return 1;
}

static void lua_val_to_bson(lua_State *L, const char *key, int vpos, bson *bs, int tref) {
    int vtype = lua_type(L, vpos);
    char nbuf[TCNUMBUFSIZ];
    if (key == NULL && vtype != LUA_TTABLE) {
        luaL_error(L, "lua_val_to_bson: Table must be on top of lua stack");
        return;
    }
    switch (vtype) {
        case LUA_TTABLE:
        {
            if (vpos < 0) {
                vpos = lua_gettop(L) + vpos + 1;
            }
            lua_checkstack(L, 3);
            int bsontype_found = luaL_getmetafield(L, vpos, "__bsontype");
            if (!bsontype_found) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, tref); //+ reg table
                lua_pushvalue(L, vpos); //+ val
                lua_rawget(L, -2); //-val +reg table val
                if (lua_toboolean(L, -1)) { //already traversed
                    lua_pop(L, 2);
                    break;
                }
                lua_pop(L, 1); //-reg table val
                lua_pushvalue(L, vpos);
                lua_pushboolean(L, 1);
                lua_rawset(L, -3);
                lua_pop(L, 1); //-reg table

                int len = 0;
                bool query = false;
                bool array = true;

                if (luaL_getmetafield(L, vpos, "__query")) {
                    lua_pop(L, 1);
                    query = true;
                    array = false;
                }
                if (array) {
                    for (lua_pushnil(L); lua_next(L, vpos); lua_pop(L, 1)) {
                        ++len;
                        if ((lua_type(L, -2) != LUA_TNUMBER) || (lua_tointeger(L, -2) != len)) {
                            lua_pop(L, 2);
                            array = false;
                            break;
                        }
                    }
                }
                if (array) {
                    if (key) bson_append_start_array(bs, key);
                    int i;
                    for (i = 1; i <= len; ++i, lua_pop(L, 1)) {
                        lua_rawgeti(L, vpos, i);
                        bson_numstrn(nbuf, TCNUMBUFSIZ, (int64_t) i);
                        lua_val_to_bson(L, nbuf, -1, bs, tref);
                    }
                    if (key) bson_append_finish_array(bs);
                } else if (query) { //special query builder case
                    //oarr format:
                    //{ {fname1, v1, v2...}, {fname2, v21, v22,..}, ... }
                    //where: vN: {op, val} OR {val} with '__bval' metafield
                    //Eg: {fname : {$inc : {...}, $dec : {...}}} -> {fname, {$inc, {}}, {$dec, {}}}

                    lua_getfield(L, vpos, "_oarr"); //+oarr
                    if (!lua_istable(L, -1)) { //it is not array
                        lua_pop(L, 1);
                        break;
                    }
                    if (key) bson_append_start_object(bs, key);
                    //iterate over _oarr
                    int ipos = lua_gettop(L);
                    size_t ilen = lua_objlen(L, ipos);
                    lua_checkstack(L, 2);
                    size_t i;
                    for (i = 1; i <= ilen; ++i, lua_pop(L, 1)) {
                        lua_rawgeti(L, ipos, i);
                        //gettop == 3
                        if (!lua_istable(L, -1)) continue;
                        char *fname = NULL;
                        int jpos = lua_gettop(L);
                        size_t jlen = lua_objlen(L, jpos);
                        lua_checkstack(L, 3);
                        bool wrapped = false;
                        size_t j;
                        for (j = 1; j <= jlen; ++j, lua_pop(L, 1)) {
                            lua_rawgeti(L, jpos, j);
                            if (j == 1) {
                                fname = strdup(lua_tostring(L, -1));
                                continue;
                            }
                            if (!fname || !lua_istable(L, -1)) { //invalid state
                                lua_pop(L, 1); //pop val
                                break;
                            }
                            int vblkpos = lua_gettop(L);
                            if (j == 2 && luaL_getmetafield(L, -1, "__bval")) { //{val} single value +metafield
                                lua_pop(L, 1); //-metafield
                                lua_rawgeti(L, vblkpos, 1); //+val
                                lua_val_to_bson(L, fname, lua_gettop(L), bs, tref);
                                lua_pop(L, 2); //-val -lua_rawgeti
                                break; //Terminate due single val
                            } else { //{op, val} value
                                if (!wrapped) {
                                    bson_append_start_object(bs, fname);
                                    wrapped = true;
                                }
                                lua_rawgeti(L, vblkpos, 1); //+op
                                const char *op = lua_tostring(L, -1);
                                if (op) {
                                    lua_rawgeti(L, vblkpos, 2); //+val
                                    lua_val_to_bson(L, op, lua_gettop(L), bs, tref);
                                    lua_pop(L, 1); //-val
                                }
                                lua_pop(L, 1); //-op
                            }
                        }
                        if (wrapped) {
                            bson_append_finish_object(bs);
                        }
                        if (fname) {
                            free(fname);
                            fname = NULL;
                        }
                    }
                    if (key) bson_append_finish_object(bs);
                    lua_pop(L, 1); //-oarr
                } else {
                    if (key) bson_append_start_object(bs, key);
                    TCLIST *keys = tclistnew();
                    //we need to sort keys due to unordered nature of lua tables
                    for (lua_pushnil(L); lua_next(L, vpos);) {
                        lua_pop(L, 1); //-val
                        size_t ksize = 0;
                        int ktype = lua_type(L, -1);
                        if (ktype == LUA_TSTRING) { //accept only string keys
                            const char* key = lua_tolstring(L, -1, &ksize);
                            tclistpush(keys, key, ksize);
                        }
                    }
                    tclistsort(keys);
                    int i;
                    for (i = 0; i < TCLISTNUM(keys); ++i) {
                        int vkeysz = TCLISTVALSIZ(keys, i);
                        const char *vkey = TCLISTVALPTR(keys, i);
                        lua_pushlstring(L, vkey, vkeysz);
                        lua_rawget(L, vpos); //+val
                        if (key == NULL && lua_type(L, -1) == LUA_TSTRING &&
                                vkeysz == JDBIDKEYNAMEL && !strcmp(JDBIDKEYNAME, vkey)) { //root level OID as string
                            //pack OID as type table
                            lua_push_bsontype_table(L, BSON_OID); //+type table
                            lua_pushvalue(L, -2); //dup oid(val) on stack
                            lua_rawseti(L, -2, 1); //pop oid val
                            if (ejdbisvalidoidstr(lua_tostring(L, -2))) {
                                lua_val_to_bson(L, vkey, lua_gettop(L), bs, tref);
                            } else {
                                luaL_error(L, "OID _id='%s' is not valid", lua_tostring(L, -2));
                            }
                            lua_pop(L, 1); //-type table
                        } else {
                            lua_val_to_bson(L, vkey, lua_gettop(L), bs, tref);
                        }
                        lua_pop(L, 1); //-val
                    }
                    tclistdel(keys);
                    if (key) bson_append_finish_object(bs);
                }
            } else { //metafield __bsontype on top
                int bson_type = lua_tointeger(L, -1);
                if (!key && bson_type != BSON_OBJECT && bson_type != BSON_ARRAY) {
                    lua_pop(L, 1);
                    luaL_error(L, "Invalid object structure");
                }
                lua_pop(L, 1); //-metafield __bsontype
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
                        if (regex && options) {
                            bson_append_regex(bs, key, regex, options);
                        }
                        lua_pop(L, 1);
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
                    case BSON_OBJECT:
                        if (key) bson_append_start_object(bs, key);
                        lua_val_to_bson(L, NULL, vpos, bs, tref);
                        if (key) bson_append_finish_object(bs);
                        break;
                    case BSON_ARRAY:
                        if (key) bson_append_start_array(bs, key);
                        lua_val_to_bson(L, NULL, vpos, bs, tref);
                        if (key) bson_append_finish_array(bs);
                        break;
                    case BSON_DOUBLE:
                        bson_append_double(bs, key, (double) lua_tonumber(L, -1));
                        break;
                    case BSON_INT:
                        bson_append_int(bs, key, (int32_t) lua_tonumber(L, -1));
                        break;
                    case BSON_LONG:
                        bson_append_long(bs, key, (int64_t) lua_tonumber(L, -1));
                        break;
                    case BSON_BOOL:
                        bson_append_bool(bs, key, lua_toboolean(L, -1));
                        break;
                    default:
                        break;
                }
                lua_pop(L, 1); //-1 first value
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
            } else {
                bson_append_double(bs, key, numval);
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
    lua_newtable(L); //+ traverse state table
    int tref = luaL_ref(L, LUA_REGISTRYINDEX); //- traverse state table into saved ref
    lua_val_to_bson(L, NULL, lua_gettop(L), bs, tref);
    lua_unref(L, tref);
}

static void bson_print_xstr(TCXSTR* xstr, const char *data, int depth) {
    bson_iterator i;
    const char *key;
    int temp;
    bson_timestamp_t ts;
    char oidhex[25];
    bson scope;
    bson_iterator_from_buffer(&i, data);

    while (bson_iterator_next(&i)) {
        bson_type t = bson_iterator_type(&i);
        if (t == 0)
            break;
        key = bson_iterator_key(&i);

        for (temp = 0; temp <= depth; temp++) {
            tcxstrprintf(xstr, ".");
        }
        tcxstrprintf(xstr, "%s(%d)=", key, t);
        switch (t) {
            case BSON_DOUBLE:
                tcxstrprintf(xstr, "%f", bson_iterator_double(&i));
                break;
            case BSON_STRING:
                tcxstrprintf(xstr, "%s", bson_iterator_string(&i));
                break;
            case BSON_SYMBOL:
                tcxstrprintf(xstr, "SYMBOL: %s", bson_iterator_string(&i));
                break;
            case BSON_OID:
                bson_oid_to_string(bson_iterator_oid(&i), oidhex);
                tcxstrprintf(xstr, "%s", oidhex);
                break;
            case BSON_BOOL:
                tcxstrprintf(xstr, "%s", bson_iterator_bool(&i) ? "true" : "false");
                break;
            case BSON_DATE:
                tcxstrprintf(xstr, "%lld", (uint64_t) bson_iterator_long(&i));
                break;
            case BSON_BINDATA:
                tcxstrprintf(xstr, "BSON_BINDATA");
                break;
            case BSON_UNDEFINED:
                tcxstrprintf(xstr, "BSON_UNDEFINED");
                break;
            case BSON_NULL:
                tcxstrprintf(xstr, "BSON_NULL");
                break;
            case BSON_REGEX:
                tcxstrprintf(xstr, "BSON_REGEX: %s", bson_iterator_regex(&i));
                break;
            case BSON_CODE:
                tcxstrprintf(xstr, "BSON_CODE: %s", bson_iterator_code(&i));
                break;
            case BSON_CODEWSCOPE:
                tcxstrprintf(xstr, "BSON_CODE_W_SCOPE: %s", bson_iterator_code(&i));
                /* bson_init( &scope ); */ /* review - stepped on by bson_iterator_code_scope? */
                bson_iterator_code_scope(&i, &scope);
                tcxstrprintf(xstr, "\n  SCOPE: ");
                bson_print_xstr(xstr, scope.data, 0);
                /* bson_destroy( &scope ); */ /* review - causes free error */
                break;
            case BSON_INT:
                tcxstrprintf(xstr, "%d", bson_iterator_int(&i));
                break;
            case BSON_LONG:
                tcxstrprintf(xstr, "%lld", (uint64_t) bson_iterator_long(&i));
                break;
            case BSON_TIMESTAMP:
                ts = bson_iterator_timestamp(&i);
                tcxstrprintf(xstr, "i: %d, t: %d", ts.i, ts.t);
                break;
            case BSON_OBJECT:
            case BSON_ARRAY:
                tcxstrprintf(xstr, "\n");
                bson_print_xstr(xstr, bson_iterator_value(&i), depth + 1);
                break;
            default:
                fprintf(stderr, "can't print type : %d\n", t);
        }
        tcxstrprintf(xstr, "\n");
    }
}
