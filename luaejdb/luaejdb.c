
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <tcejdb/ejdb.h>

#define EJDBLIBNAME	"luaejdb"
#define EJDBUDATAKEY "__ejdb__"
#define EJDBUDATAMT "mt.ejdb.db"

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

#define EJDBERR(_L, _DB) \
    return luaL_error((_L), "EJDB ERROR %d|%s", ejdbecode(_DB), ejdberrmsg(ejdbecode(_DB)))

static void initdbconsts(lua_State *L) {
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
}

int dbdel(lua_State *L) {
    EJDBDATA *data = luaL_checkudata(L, 1, EJDBUDATAMT);
    EJDB *db = data->db;
    if (db) {
        ejdbdel(db);
        data->db = NULL;
    }
    return 0;
}

int dbclose(lua_State *L) {
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

int find(lua_State *L) {


    
}




int dbopen(lua_State *L) {
    int argc = lua_gettop(L);
    const char *path = luaL_checkstring(L, 1);
    int mode = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : DEFAULT_OPEN_MODE;

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
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, EJDBUDATAKEY); //pop userdata

    //Metatable
    lua_newtable(L);
    lua_pushcfunction(L, dbdel);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);

    //Add constants
    initdbconsts(L);

    //Add methods
    lua_pushcfunction(L, dbclose);
    lua_setfield(L, -2, "close");

    if (lua_gettop(L) - argc != 1) {
        ejdbdel(db);
        udb->db = NULL;
        return luaL_error(L, "Invalid stack state %d", lua_gettop(L));
    }
    return 1;
}

/* Init */
int luaopen_luaejdb(lua_State *L) {
    lua_settop(L, 0);
    lua_newtable(L);
    initdbconsts(L);
    lua_pushstring(L, "open");
    lua_pushcfunction(L, dbopen);
    lua_settable(L, -3);
    return 1;
}



