
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <tcejdb/ejdb.h>

#define LUA_EJDBLIBNAME	"luaejdb"

int dbopen(lua_State *L) {
    lua_pushstring(L, "Ha ha ejdb");
    return 1;
}

static const luaL_Reg ejdblib[] = {
    "open", dbopen,
    {NULL}
};

/* Init */
int luaopen_luaejdb(lua_State *L) {
    luaL_newlib(L, ejdblib);
    return 1;
}



