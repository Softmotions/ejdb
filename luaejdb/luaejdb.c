
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <tcejdb/ejdb.h>

#include "luabson.h"

#define EJDBLIBNAME	"luaejdb"
#define EJDBUDATAKEY "__ejdb__"
#define EJDBUDATAMT "mt.ejdb.db"
#define EJDBCURSORMT "mt.ejdb.cur"

#define DEFAULT_OPEN_MODE (JBOWRITER | JBOCREAT | JBOTSYNC)

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

typedef struct {
    EJDB *db;
} EJDBDATA;

typedef struct {
    TCLIST *res;
} CURSORDATA;

#define EJDBERR(_L, _DB) \
    return luaL_error((_L), "EJDB ERROR %d|%s", ejdbecode(_DB), ejdberrmsg(ejdbecode(_DB)))

static void init_db_consts(lua_State *L) {
    if (!lua_istable(L, -1)) {
        luaL_error(L, "Table must be on top of lua stack");
    }
    TBLSETNUMCONST(JBOREADER);
    TBLSETNUMCONST(JBOWRITER);
    TBLSETNUMCONST(JBOCREAT);
    TBLSETNUMCONST(JBOTRUNC);
    TBLSETNUMCONST(JBONOLCK);
    TBLSETNUMCONST(JBOLCKNB);
    TBLSETNUMCONST(JBOTSYNC);
    TBLSETNUMCONST(JBIDXDROP);
    TBLSETNUMCONST(JBIDXDROPALL);
    TBLSETNUMCONST(JBIDXOP);
    TBLSETNUMCONST(JBIDXREBLD);
    TBLSETNUMCONST(JBIDXNUM);
    TBLSETNUMCONST(JBIDXSTR);
    TBLSETNUMCONST(JBIDXISTR);
    TBLSETNUMCONST(JBIDXARR);
    TBLSETNUMCONST(JBQRYCOUNT);
    TBLSETNUMCONST(DEFAULT_OPEN_MODE);

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
    if (cdata->res) {
        tclistdel(cdata->res);
        cdata->res = NULL;
    }
    return 0;
}

static int cursor_field(lua_State *L) {
    return 0;
}

static int cursor_object(lua_State *L) {
    return 0;
}

static int cursor_iter(lua_State *L) {
    return 0;
}

static int db_del(lua_State *L) {
    EJDBDATA *data = luaL_checkudata(L, 1, EJDBUDATAMT);
    EJDB *db = data->db;
    if (db) {
        ejdbdel(db);
        data->db = NULL;
    }
    return 0;
}

static int db_close(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    EJDB *db = data->db;
    if (db) {
        if (!ejdbclose(db)) {
            EJDBERR(L, db);
        }
    }
    return 0;
}

static int db_find(lua_State *L) {
    //cname, q.toBSON(), orBsons, q.toHintsBSON(), flags
    luaL_checktype(L, 1, LUA_TTABLE); //self
    lua_getfield(L, 1, EJDBUDATAKEY);
    EJDBDATA *data = luaL_checkudata(L, -1, EJDBUDATAMT);
    const char *cname = luaL_checkstring(L, 2); //collections name
    const char *qbsonbuf = luaL_checkstring(L, 3); //Query bson
    luaL_checktype(L, 4, LUA_TTABLE); //or joined
    const char *hbsonbuf = luaL_checkstring(L, 5); //Hints bson
    if (!data || !data->db || !qbsonbuf || !hbsonbuf) {
        return luaL_error(L, "Illegal arguments");
    }
    bson oqarrstack[8]; //max 8 $or bsons on stack
    bson *oqarr = NULL;
    bson qbson = {NULL};
    bson hbson = {NULL};
    EJQ *q = NULL;
    EJDB *db = data->db;
    EJCOLL *coll = NULL;
    uint32_t count = 0;

    //lua_objlen(L, 4)

    //bson_print_raw(stderr, hbson, 0);

    return 0;
}

static int db_open(lua_State *L) {
    int argc = lua_gettop(L);
    const char *path = luaL_checkstring(L, 1);
    int mode = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : DEFAULT_OPEN_MODE;

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

    lua_pushcfunction(L, db_find);
    lua_setfield(L, -2, "_find");

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

/* Init */
int luaopen_luaejdb(lua_State *L) {
    lua_settop(L, 0);

    lua_newtable(L);
    lua_init_bson(L);
    init_db_consts(L);

    lua_pushcfunction(L, db_open);
    lua_setfield(L, -2, "open");

    //Push cursor methods into metatable
    luaL_newmetatable(L, EJDBCURSORMT);
    lua_newtable(L);
    lua_pushcfunction(L, cursor_object);
    lua_setfield(L, -2, "object");
    lua_pushcfunction(L, cursor_field);
    lua_setfield(L, -2, "field");
    lua_setfield(L, -2, "__index");
    lua_settop(L, 1);


    return 1;
}



