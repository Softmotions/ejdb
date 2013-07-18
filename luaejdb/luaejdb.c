
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <tcejdb/ejdb_private.h>

#include "luabson.h"

#include <string.h>

//EJDB db user data
#define EJDBUDATAKEY "__ejdb__"
#define EJDBUDATAMT "mtejdb"
//Cursor user data
#define EJDBCURSORMT "mtejc"

#define DEFAULT_OPEN_MODE (JBOWRITER | JBOCREAT)

#define TBLSETNUMCONST(_CN) \
    do { \
        lua_pushstring(L, #_CN); \
        lua_pushnumber(L, (_CN)); \
        lua_settable(L, -3); \
    } while(0)

#define TBLSETCFUN(_CN, _CF) \
    do { \
        lua_pushstring(L, (_CN); \
        lua_pushcfunction(L, (_CF)); \
        lua_settable(L, -3); \
    } while(0)

enum {
    JBQRYLOG = 1 << 10 //return query execution log string
};

typedef struct {
    EJDB *db;
} EJDBDATA;

typedef struct {
    TCLIST *qres;
} CURSORDATA;

#define EJDBERR(_L, _DB) \
    return luaL_error((_L), "EJDB ERROR %d|%s", ejdbecode(_DB), ejdberrmsg(ejdbecode(_DB)))

static int cursor_object(lua_State *L);
static int cursor_index(lua_State *L);

static int set_ejdb_error(lua_State *L, EJDB *jb) {
    int ecode = ejdbecode(jb);
    const char *emsg = ejdberrmsg(ecode);
    return luaL_error(L, emsg);
}

static EJDB *check_ejdb(lua_State *L, EJDB *jb) {
    if (jb == NULL) {
        luaL_error(L, "Closed EJDB database");
    }
    return jb;
}

static void init_db_consts(lua_State *L) {
    if (!lua_istable(L, -1)) {
        luaL_error(L, "Table must be on top of lua stack");
    }
    TBLSETNUMCONST(BSON_DOUBLE);
    TBLSETNUMCONST(BSON_STRING);
    TBLSETNUMCONST(BSON_OBJECT);
    TBLSETNUMCONST(BSON_ARRAY);
    TBLSETNUMCONST(BSON_BINDATA);
    TBLSETNUMCONST(BSON_UNDEFINED);
    TBLSETNUMCONST(BSON_OID);
    TBLSETNUMCONST(BSON_BOOL);
    TBLSETNUMCONST(BSON_DATE);
    TBLSETNUMCONST(BSON_NULL);
    TBLSETNUMCONST(BSON_REGEX);
    TBLSETNUMCONST(BSON_CODE);
    TBLSETNUMCONST(BSON_SYMBOL);
    TBLSETNUMCONST(BSON_INT);
    TBLSETNUMCONST(BSON_LONG);
}

static int cursor_del(lua_State *L) {
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    if (cdata->qres) {
        tclistdel(cdata->qres);
        cdata->qres = NULL;
    }
    return 0;
}

static int cursor_field(lua_State *L) {
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    TCLIST *qres = cdata->qres;
    if (!qres) {
        return luaL_error(L, "Cursor closed");
    }
    int idx = luaL_checkinteger(L, 2);
    const char *fname = luaL_checkstring(L, 3);
    int sz = TCLISTNUM(qres);
    if (idx < 0) {
        idx = sz - idx + 1;
    }
    if (idx > 0 && idx <= sz) {
        const void *bsbuf = TCLISTVALPTR(qres, idx - 1);
        bson_iterator it;
        if (bson_find_from_buffer(&it, bsbuf, fname) != BSON_EOO) {
            lua_push_bson_value(L, &it);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int cursor_object(lua_State *L) {
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    TCLIST *qres = cdata->qres;
    if (!qres) {
        return luaL_error(L, "Cursor closed");
    }
    int idx = luaL_checkinteger(L, 2);
    int sz = TCLISTNUM(qres);
    if (idx < 0) {
        idx = sz - idx + 1;
    }
    if (idx > 0 && idx <= sz) {
        const void *bsbuf = TCLISTVALPTR(qres, idx - 1);
        bson_iterator it;
        bson_iterator_from_buffer(&it, bsbuf);
        lua_push_bson_table(L, &it);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int cursor_index(lua_State *L) {
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    TCLIST *qres = cdata->qres;
    const int atype = lua_type(L, 2);
    if (atype == LUA_TNUMBER) { //access by index
        if (!qres) {
            return luaL_error(L, "Cursor closed");
        }
        int idx = lua_tointeger(L, 2);
        int sz = TCLISTNUM(qres);
        if (idx < 0) {
            idx = sz - idx + 1;
        }
        if (idx > 0 && idx <= sz) {
            lua_pushlstring(L, TCLISTVALPTR(qres, idx - 1), TCLISTVALSIZ(qres, idx - 1));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } else if (atype == LUA_TSTRING) {
        const char *op = lua_tostring(L, 2);
        if (!strcmp(op, "field")) {
            lua_pushcfunction(L, cursor_field);
            return 1;
        } else if (!strcmp(op, "object")) {
            lua_pushcfunction(L, cursor_object);
            return 1;
        } else if (!strcmp(op, "close")) {
            lua_pushcfunction(L, cursor_del);
            return 1;
        }
    }
    return 0;
}

static int cursor_len(lua_State *L) {
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    if (!cdata->qres) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, (cdata->qres) ? TCLISTNUM(cdata->qres) : 0);
    return 1;
}

static int cursor_iter_next(lua_State *L) {
    CURSORDATA *cdata = lua_touserdata(L, lua_upvalueindex(1));
    int idx = luaL_checkinteger(L, lua_upvalueindex(2));
    TCLIST *qres = cdata->qres;
    if (!qres) {
        return 0;
    }
    int sz = TCLISTNUM(qres);
    if (idx > 0 && idx <= sz) {
        const void *bsbuf = TCLISTVALPTR(qres, idx - 1);
        bson_iterator it;
        bson_iterator_from_buffer(&it, bsbuf);
        lua_push_bson_table(L, &it);
        lua_pushinteger(L, idx + 1);
        lua_replace(L, lua_upvalueindex(2));
        lua_pushinteger(L, idx);
        return 2; //obj, idx
    } else {
        return 0;
    }
};

static int cursor_iter_bsons_next(lua_State *L) {
    CURSORDATA *cdata = lua_touserdata(L, lua_upvalueindex(1));
    int idx = luaL_checkinteger(L, lua_upvalueindex(2));
    TCLIST *qres = cdata->qres;
    if (!qres) {
        return 0;
    }
    int sz = TCLISTNUM(qres);
    if (idx > 0 && idx <= sz) {
        lua_pushlstring(L, TCLISTVALPTR(qres, idx - 1), TCLISTVALSIZ(qres, idx - 1));
        lua_pushinteger(L, idx + 1);
        lua_replace(L, lua_upvalueindex(2));
        lua_pushinteger(L, idx);
        return 2; //bson, idx
    } else {
        return 0;
    }
};

static int cursor_call(lua_State *L) {
    const char *flags = "";
    CURSORDATA *cdata = luaL_checkudata(L, 1, EJDBCURSORMT);
    if (lua_isstring(L, 2)) {
        flags = lua_tostring(L, 2);
    }
    lua_pushlightuserdata(L, cdata);
    lua_pushinteger(L, 1);
    if (!strcmp(flags, "raw")) {
        lua_pushcclosure(L, cursor_iter_bsons_next, 2);
    } else {
        lua_pushcclosure(L, cursor_iter_next, 2);
    }
    return 1;
}

static int db_del(lua_State *L) {
    EJDBDATA *data = luaL_checkudata(L, 1, EJDBUDATAMT);
    EJDB *jb = data->db;
    if (jb) {
        ejdbdel(jb);
        data->db = NULL;
    }
    return 0;
}

static int db_close(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *jb = data->db;
    if (jb) {
        if (!ejdbclose(jb)) {
            EJDBERR(L, jb);
        }
    }
    return 0;
}

static int db_is_open(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    lua_pushboolean(L, ejdbisopen(((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db));
    return 1;
}

static int db_ensure_collection(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *jb = data->db;
    const char *cname = luaL_checkstring(L, 2);
    EJCOLLOPTS opts = {NULL};
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "cachedrecords");
        if (lua_isnumber(L, -1)) {
            opts.cachedrecords = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "records");
        if (lua_isnumber(L, -1)) {
            opts.records = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "large");
        opts.large = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "compressed");
        opts.compressed = lua_toboolean(L, -1);
        lua_pop(L, 1); //-field
    }
    if (!ejdbcreatecoll(jb, cname, &opts)) {
        return set_ejdb_error(L, jb);
    }
    return 0;
}

static int db_drop_collection(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *jb = data->db;
    const char *cname = luaL_checkstring(L, 2);
    bool unlink = lua_toboolean(L, 3);
    if (!ejdbrmcoll(jb, cname, unlink)) {
        return set_ejdb_error(L, jb);
    }
    return 0;
}

static int db_remove(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *jb = data->db;
    bson_oid_t oid;
    memset(&oid, 0, sizeof (oid));
    const char *cname = luaL_checkstring(L, 2); //collections name
    if (lua_isstring(L, 3)) {
        const char *soid = lua_tostring(L, 3);
        if (ejdbisvalidoidstr(soid)) {
            bson_oid_from_string(&oid, soid);
        }
    } else if (luaL_getmetafield(L, 3, "__bsontype") && lua_tointeger(L, -1) == BSON_OID) {
        lua_pop(L, 1);
        lua_rawgeti(L, 3, 1);
        const char *soid = lua_tostring(L, -1);
        if (ejdbisvalidoidstr(soid)) {
            bson_oid_from_string(&oid, soid);
        }
        lua_pop(L, 1);
    }
    if (!oid.ints[0] && !oid.ints[1] && !oid.ints[2]) {
        return luaL_error(L, "Invalid OID arg #2");
    }
    EJCOLL *coll = ejdbgetcoll(jb, cname);
    if (!coll) {
        return 0;
    }
    if (!ejdbrmbson(coll, &oid)) {
        return set_ejdb_error(L, jb);
    }
    return 0;
}

static int db_sync(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *jb = data->db;
    check_ejdb(L, jb);
    if (lua_isstring(L, 2)) { //sync coll
        const char *cname = lua_tostring(L, 2);
        EJCOLL *coll = ejdbgetcoll(jb, cname);
        if (coll) {
            if (!ejdbsyncoll(coll)) {
                return set_ejdb_error(L, jb);
            }
        }
    } else { //sync db
        if (!ejdbsyncdb(jb)) {
            return set_ejdb_error(L, jb);
        }
    }
    return 0;
}

static int db_set_index(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    const char *cname = luaL_checkstring(L, 2); //collections name
    const char *fpath = luaL_checkstring(L, 3); //index field path
    //index types:
    // s: string
    // i: ignore case string index
    // n: number index
    // a: array toke index
    const char *itypes = luaL_checkstring(L, 4);
    const char *iop = luaL_checkstring(L, 5); //index operation

    int i;
    uint32_t iflags = 0;
    for (i = 0; itypes[i] != '\0'; ++i) {
        switch (itypes[i]) {
            case 's':
                iflags |= JBIDXSTR;
                break;
            case 'i':
                iflags |= JBIDXISTR;
                break;
            case 'n':
                iflags |= JBIDXNUM;
                break;
            case 'a':
                iflags |= JBIDXARR;
                break;
            case '*':
                iflags |= (JBIDXSTR | JBIDXISTR | JBIDXNUM | JBIDXARR);
                break;
        }
    }

    for (i = 0; iop[i] != '\0'; ++i) {
        switch (iop[i]) {
            case 'd':
                iflags |= JBIDXDROP;
                break;
            case 'a':
                iflags |= JBIDXDROPALL;
                break;
            case 'o':
                iflags |= JBIDXOP;
                break;
            case 'r':
                iflags |= JBIDXREBLD;
                break;
        }
    }

    EJCOLL *coll = ejdbgetcoll(jb, cname);
    if (!coll) {
        return 0;
    }
    if (!ejdbsetindex(coll, fpath, iflags)) {
        return set_ejdb_error(L, jb);
    }
    return 0;
}

static int db_txctl(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    const char *cname = luaL_checkstring(L, 2); //collections name
    const char *cmd = luaL_checkstring(L, 3); //command
    EJCOLL *coll = ejdbcreatecoll(jb, cname, NULL);
    if (!coll) {
        return set_ejdb_error(L, jb);
    }
    int i;
    bool jbret = true;
    for (i = 0; cmd[i] != '\0'; ++i) {
        switch (cmd[i]) {
            case 'b': //begin transaction
                jbret = ejdbtranbegin(coll);
                break;
            case 'c':
                jbret = ejdbtrancommit(coll);
                break;
            case 'a':
                jbret = ejdbtranabort(coll);
                break;
            case 's':
            {
                bool status;
                jbret = ejdbtranstatus(coll, &status);
                if (jbret) {
                    lua_pushboolean(L, status);
                    return 1;
                }
            }
        }
    }
    if (!jbret) {
        return set_ejdb_error(L, jb);
    }
    return 0;
}

static int db_meta(lua_State *L) {
    int argc = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    lua_pop(L, 1);
    bson *meta = ejdbmeta(jb);
    if (meta == NULL) {
        lua_pushnil(L);
        return 1;
    }
    bson_iterator it;
    bson_iterator_init(&it, meta);
    lua_push_bson_table(L, &it);
    bson_del(meta);
    if (lua_gettop(L) - argc != 1) {
        return luaL_error(L, "db_meta: Invalid stack size: %d should be: %d", (lua_gettop(L) - argc), 1);
    }
    return 1;
}

static int db_version(lua_State *L) {
    lua_pushstring(L, ejdbversion());
    return 1;
}

static int db_save(lua_State *L) {
    int argc = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    lua_pop(L, 1);
    const char *cname = luaL_checkstring(L, 2); //collections name
    bson bsonval;
    const char *bsonbuf = luaL_checkstring(L, 3); //Object to save
    bson_init_finished_data(&bsonval, bsonbuf);
    bsonval.flags |= BSON_FLAG_STACK_ALLOCATED;
    bool merge = false;
    if (lua_isboolean(L, 4)) { //merge flag
        merge = lua_toboolean(L, 4);
    }
    EJCOLL *coll = ejdbcreatecoll(jb, cname, NULL);
    if (!coll) {
        return set_ejdb_error(L, jb);
    }
    bson_oid_t oid;
    if (!ejdbsavebson2(coll, &bsonval, &oid, merge)) {
        bson_destroy(&bsonval);
        return set_ejdb_error(L, jb);
    }
    char xoid[25];
    bson_oid_to_string(&oid, xoid);
    lua_pushstring(L, xoid);

    bson_destroy(&bsonval);
    if (lua_gettop(L) - argc != 1) { //got +lua_getfield(L, 1, EJDBUDATAKEY)
        return luaL_error(L, "db_save: Invalid stack size: %d should be: %d", (lua_gettop(L) - argc), 1);
    }
    return 1;
}

static int db_command(lua_State *L) {
    int argc = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    lua_pop(L, 1);
    bson bsonval;
    const char *bsonbuf = luaL_checkstring(L, 2); //Object to save
    bson_init_finished_data(&bsonval, bsonbuf);
    bsonval.flags |= BSON_FLAG_STACK_ALLOCATED;
    bson *bs = ejdbcommand(jb, &bsonval);
    if (!bs) {
        lua_pushnil(L);
        goto finish;
    }
    bson_iterator it;
    bson_iterator_init(&it, bs);
    lua_push_bson_table(L, &it);
    bson_del(bs);

finish:
    if (lua_gettop(L) - argc != 1) { //got
        return luaL_error(L, "db_command: Invalid stack size: %d should be: %d", (lua_gettop(L) - argc), 1);
    }
    bson_destroy(&bsonval);
    return 1;
}


static int db_load(lua_State *L) {
    int argc = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    lua_pop(L, 1);

    const char *cname = luaL_checkstring(L, 2);
    bson_oid_t oid;
    memset(&oid, 0, sizeof (oid));

    if (lua_type(L, 3) == LUA_TSTRING) {
        const char *soid = lua_tostring(L, 3);
        if (ejdbisvalidoidstr(soid)) {
            bson_oid_from_string(&oid, soid);
        }
    } else if (luaL_getmetafield(L, 3, "__bsontype") && lua_tointeger(L, -1) == BSON_OID) {
        lua_pop(L, 1);
        lua_rawgeti(L, 3, 1);
        const char *soid = lua_tostring(L, -1);
        if (ejdbisvalidoidstr(soid)) {
            bson_oid_from_string(&oid, soid);
        }
        lua_pop(L, 1);
    }
    if (!oid.ints[0] && !oid.ints[1] && !oid.ints[2]) {
        return luaL_error(L, "Invalid OID arg #2");
    }
    EJCOLL *coll = ejdbgetcoll(jb, cname);
    if (!coll) {
        lua_pushnil(L);
        goto finish;
    }
    bson *bs = ejdbloadbson(coll, &oid);
    if (!bs) {
        lua_pushnil(L);
        goto finish;
    }
    bson_iterator it;
    bson_iterator_init(&it, bs);
    lua_push_bson_table(L, &it);
    bson_del(bs);

finish:
    if (lua_gettop(L) - argc != 1) { //got
        return luaL_error(L, "db_load: Invalid stack size: %d should be: %d", (lua_gettop(L) - argc), 1);
    }
    return 1;
}

static int db_find(lua_State *L) {
    //cname, q.toBSON(), orBsons, q.toHintsBSON(), flags
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDB *jb = ((EJDBDATA*) luaL_checkudata(L, -1, EJDBUDATAMT))->db;
    check_ejdb(L, jb);
    lua_pop(L, 1);
    const char *cname = luaL_checkstring(L, 2); //collections name
    const char *qbsbuf = luaL_checkstring(L, 3); //Query bson
    luaL_checktype(L, 4, LUA_TTABLE); //or joined
    const char *hbsbuf = luaL_checkstring(L, 5); //Hints bson
    int qflags = 0;
    if (lua_isstring(L, 6)) {
        const char *sflags = lua_tostring(L, 6);
        int i;
        for (i = 0; sflags[i] != '\0'; ++i) {
            switch (sflags[i]) {
                case 'c':
                    qflags |= JBQRYCOUNT;
                    break;
                case 'l':
                    qflags |= JBQRYLOG;
                    break;
            }
        }
    }

    size_t i;
    bson oqarrstack[8]; //max 8 $or bsons on stack
    bson *oqarr = NULL;
    bson qbson = {NULL};
    bson hbson = {NULL};
    EJQ *q = NULL;
    EJCOLL *coll = NULL;
    uint32_t count = 0;
    TCLIST *qres = NULL;
    bool jberr = false;
    TCXSTR *log = NULL;

    size_t orsz = lua_objlen(L, 4);
    oqarr = ((orsz <= 8) ? oqarrstack : (bson*) tcmalloc(orsz * sizeof (bson)));


    for (i = 0; i < orsz; ++i) {
        const void *bsdata;
        size_t bsdatasz;
        lua_rawgeti(L, 4, i + 1);
        if (lua_type(L, -1) != LUA_TSTRING) {
            return luaL_error(L, "Invalid 'or' array. Arg #3");
        }
        bsdata = lua_tolstring(L, -1, &bsdatasz);
        bson_init_finished_data(&oqarr[i], bsdata);
        oqarr[i].flags |= BSON_FLAG_STACK_ALLOCATED; //prevent bson data to be freed in bson_destroy
    }

    bson_init_finished_data(&qbson, qbsbuf);
    qbson.flags |= BSON_FLAG_STACK_ALLOCATED;
    bson_init_finished_data(&hbson, hbsbuf);
    hbson.flags |= BSON_FLAG_STACK_ALLOCATED;

    q = ejdbcreatequery(jb, &qbson, orsz > 0 ? oqarr : NULL, orsz, &hbson);
    if (!q) {
        jberr = true;
        goto finish;
    }
    coll = ejdbgetcoll(jb, cname);
    if (!coll) {
        bson_iterator it;
        //If we are in $upsert mode a new collection will be created
        if (bson_find(&it, &qbson, "$upsert") == BSON_OBJECT) {
            coll = ejdbcreatecoll(jb, cname, NULL);
            if (!coll) {
                jberr = true;
                goto finish;
            }
        }
    }
    if (!coll) { //No collection -> no results
        qres = (qflags & JBQRYCOUNT) ? NULL : tclistnew2(1); //empty results
    } else {
        if (qflags & JBQRYLOG) {
            log = tcxstrnew();
        }
        qres = ejdbqryexecute(coll, q, &count, qflags, log);
        if (ejdbecode(jb) != TCESUCCESS) {
            jberr = true;
            goto finish;
        }
    }

    //+query res
    if (qres) {
        CURSORDATA *cdata = lua_newuserdata(L, sizeof (*cdata));
        cdata->qres = qres;
        luaL_newmetatable(L, EJDBCURSORMT);
        lua_setmetatable(L, -2);
    } else {
        lua_pushnil(L);
    }

    //+count
    lua_pushinteger(L, count);

    //+log
    if (log) {
        lua_pushlstring(L, TCXSTRPTR(log), TCXSTRSIZE(log));
    } else {
        lua_pushnil(L);
    }

finish:

    for (i = 0; i < orsz; ++i) {
        bson_destroy(&oqarr[i]);
    }
    if (oqarr && oqarr != oqarrstack) {
        tcfree(oqarr);
    }
    bson_destroy(&qbson);
    bson_destroy(&hbson);
    if (q) {
        ejdbquerydel(q);
    }
    if (log) {
        tcxstrdel(log);
    }
    if (jberr) {
        return set_ejdb_error(L, jb);
    }

    return 3; //qres, count, log
}

static int db_open(lua_State *L) {
    int argc = lua_gettop(L);
    const char *path = luaL_checkstring(L, 1);
    int mode = DEFAULT_OPEN_MODE;
    if (lua_isnumber(L, 2)) {
        mode = lua_tointeger(L, 2);
    } else if (lua_isstring(L, 2)) {
        mode = 0;
        const char* om = lua_tostring(L, 2);
        int i;
        for (i = 0; om[i] != '\0'; ++i) {
            mode |= JBOREADER;
            switch (om[i]) {
                case 'w':
                    mode |= JBOWRITER;
                    break;
                case 'c':
                    mode |= JBOCREAT;
                    break;
                case 't':
                    mode |= JBOTRUNC;
                    break;
                case 's':
                    mode |= JBOTSYNC;
                    break;
            }
        }
    }

    //DB table
    lua_newtable(L);
    EJDB *db = ejdbnew();
    if (!db) {
        return luaL_error(L, "Unable to create db instance");
    }
    if (!ejdbopen(db, path, mode)) {
        ejdbdel(db);
        EJDBERR(L, db);
    }

    EJDBDATA *udb = lua_newuserdata(L, sizeof (*udb));
    udb->db = db;
    luaL_newmetatable(L, EJDBUDATAMT);
    lua_pushcfunction(L, db_del);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, EJDBUDATAKEY); //pop userdata

    //Add constants
    init_db_consts(L);

    //Add methods
    lua_pushcfunction(L, db_close);
    lua_setfield(L, -2, "close");

    lua_pushcfunction(L, db_save);
    lua_setfield(L, -2, "_save");

    lua_pushcfunction(L, db_load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, db_command);
    lua_setfield(L, -2, "_command");

    lua_pushcfunction(L, db_find);
    lua_setfield(L, -2, "_find");

    lua_pushcfunction(L, db_is_open);
    lua_setfield(L, -2, "isOpen");

    lua_pushcfunction(L, db_ensure_collection);
    lua_setfield(L, -2, "ensureCollection");

    lua_pushcfunction(L, db_drop_collection);
    lua_setfield(L, -2, "dropCollection");

    lua_pushcfunction(L, db_remove);
    lua_setfield(L, -2, "remove");

    lua_pushcfunction(L, db_sync);
    lua_setfield(L, -2, "sync");

    lua_pushcfunction(L, db_set_index);
    lua_setfield(L, -2, "_setIndex");

    lua_pushcfunction(L, db_txctl);
    lua_setfield(L, -2, "_txctl");

    lua_pushcfunction(L, db_meta);
    lua_setfield(L, -2, "getDBMeta");

    if (lua_gettop(L) - argc != 1) {
        ejdbdel(db);
        udb->db = NULL;
        return luaL_error(L, "Invalid stack state %d", lua_gettop(L));
    }
    if (!lua_istable(L, -1)) {
        ejdbdel(db);
        udb->db = NULL;
        return luaL_error(L, "Table must be on top of lua stack");
    }
    return 1;
}

static const struct luaL_Reg ejdbcursor_m[] = {
    {"__index", cursor_index},
    {"__len", cursor_len},
    {"__call", cursor_call},
    {"__gc", cursor_del},
    {NULL, NULL}
};

/* Init */
int luaopen_luaejdb(lua_State *L) {
    lua_settop(L, 0);

    lua_newtable(L);
    lua_init_bson(L);
    init_db_consts(L);

    lua_pushcfunction(L, db_open);
    lua_setfield(L, -2, "open");

    lua_pushcfunction(L, db_version);
    lua_setfield(L, -2, "version");

    //Push cursor methods into metatable
    luaL_newmetatable(L, EJDBCURSORMT);
    luaL_register(L, NULL, ejdbcursor_m);
    lua_pop(L, 1);

    lua_settop(L, 1);
    return 1;
}
