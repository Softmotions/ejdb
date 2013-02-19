
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <tcejdb/ejdb.h>

#define LUA_EJDBLIBNAME	"ejdb"


static const luaL_Reg ejdblib[] = {
    "open", open,
    {NULL}
};

int open(lua_State *L) {
    lua_pushstring(L, "Ha ha ejdb");
    return 1;
}

/* Init */
int luaopen_ejdb(lua_State *L) {
    luaL_newlib(L, ejdblib);
    return 1;
}



