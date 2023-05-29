#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "luajit/src/lauxlib.h"
#include "luajit/src/lua.h"
#include "luajit/src/lualib.h"

#ifdef __cplusplus
}
#endif

void lua_check_err(lua_State* L, int err) {
  if (err == 0) return;

  std::cout << "lua_pcall failed!" << std::endl;
  switch (err) {
    case LUA_ERRRUN:
      std::cout << "Runtime error" << std::endl;
      break;
    case LUA_ERRMEM:
      std::cout << "Memory allocation error" << std::endl;
      break;
    case LUA_ERRERR:
      std::cout << "Error while handling error" << std::endl;
      break;
    default:
      std::cout << "Other error" << std::endl;
  }
  std::cout << "Error message: " << std::endl;
  std::cout << lua_tostring(L, -1) << std::endl;
}

int main(int argc, const char** argv) {
  if (argc < 4) {
    std::cout
        << "Usage: <executable> <lua_file> <err_lua_func> <correct_lua_func>"
        << std::endl;
    return -1;
  }
  const char* file_name = argv[1];
  const char* err_func_name = argv[2];
  const char* correct_func_name = argv[3];

  lua_State* L = luaL_newstate();  // create a new lua instance
  luaL_openlibs(L);                // give lua access to basic libraries

  luaL_dofile(L, file_name);

  lua_getglobal(L, err_func_name);
  lua_pushstring(L, "a");
  lua_pushstring(L, "b");
  lua_pushstring(L, "c");
  int res = lua_pcall(L, 3, 0, 0);
  lua_check_err(L, res);

  lua_getglobal(L, correct_func_name);
  lua_pushstring(L, "a");
  lua_pushstring(L, "b");
  lua_pushstring(L, "c");
  res = lua_pcall(L, 3, 0, 0);
  lua_check_err(L, res);

  return 0;
}
