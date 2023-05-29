#include <cassert>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

#include "test_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "luajit/src/lauxlib.h"
#include "luajit/src/lua.h"
#include "luajit/src/lualib.h"

#ifdef __cplusplus
}
#endif

int lua_print_data_dict(lua_State* L) {
  DataDict** pptr =
      static_cast<DataDict**>(luaL_checkudata(L, 1, "DataDictMetaTable"));
  if (pptr == nullptr) {
    std::cout << "print_data_dict null ptr" << std::endl;
  }
  std::cout << "Data dict contents in lua: " << std::endl;
  for (const auto& key : {"a", "b", "c", "d"}) {
    std::cout << key << ": " << (*pptr)->field(key) << std::endl;
  }
  return 0;
}

int lua_data_dict_field(lua_State* L) {
  DataDict** pptr =
      static_cast<DataDict**>(luaL_checkudata(L, 1, "DataDictMetaTable"));
  if (pptr == nullptr) {
    std::cout << "field null ptr" << std::endl;
  }
  const char* field = luaL_checkstring(L, 2);
  lua_pushnumber(L, (*pptr)->field(field));
  return 1;
}

int lua_data_dict_set_field(lua_State* L) {
  DataDict** pptr =
      static_cast<DataDict**>(luaL_checkudata(L, 1, "DataDictMetaTable"));
  if (pptr == nullptr) {
    std::cout << "set_field null ptr" << std::endl;
  }
  const char* field = luaL_checkstring(L, 2);
  double value = luaL_checknumber(L, 3);
  (*pptr)->set_field(field, value);
  return 0;
}

void lua_register_data_dict(lua_State* L) {
  int flag = luaL_newmetatable(L, "DataDictMetaTable");
  if (!flag) {
    // Metatable has already created. Report an error.
    std::cout << "Meta table has already been created!" << std::endl;
    // NOTICE: Even if it has been created before, the created table is copied
    // onto the top of stack.
    return;
  }
  // Register member functions

  // `luaL_newmetatable` creates a new table on top of stack (pos = -1).
  // Copy it again on the stack (push the same value on pos = -1).
  lua_pushvalue(L, -1);
  // After copy, now both -1 and -2 are metatables.
  // Belowing `lua_setfield` pops the top entry (pos = -1).
  // Then assign it at the second last top entry's (pos = -2) "__index"
  // metamethod. Therefore Lua can find member functions through "__index".
  lua_setfield(L, -2, "__index");

  luaL_Reg data_dict_funcs[] = {{"field", lua_data_dict_field},
                                {"set_field", lua_data_dict_set_field},
                                {"print_info", lua_print_data_dict},
                                {nullptr, nullptr}};

  // Register all functions into the table on the top of stack.
  luaL_register(L, nullptr, data_dict_funcs);
}

void lua_push_data_dict(lua_State* L, DataDict* ptr) {
  DataDict** pptr =
      static_cast<DataDict**>(lua_newuserdata(L, sizeof(DataDict*)));
  assert(pptr != nullptr);
  *pptr = ptr;
  // Copy the metatable onto the top of stack.
  int flag = luaL_newmetatable(L, "DataDictMetaTable");
  assert(flag == 0);
  // Pop metatable and set it for second last entry.
  lua_setmetatable(L, -2);
}

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    std::cout << "Usage: <executable> <lua_file> <lua_func>"
              << std::endl;
    return -1;
  }
  const char* file_name = argv[1];
  const char* func_name = argv[2];

  Data data;
  DataDict dict(data);

  std::cout << "Data dict constents in c++: " << std::endl;
  for (const auto& field : {"a", "b", "c", "d"}) {
    std::cout << field << ": " << dict.field(field) << std::endl;
  }

  lua_State* L = luaL_newstate();  // create a new lua instance
  luaL_openlibs(L);                // give lua access to basic libraries

  // Register user defined functions.
  lua_register_data_dict(L);

  luaL_dofile(L, file_name);

  lua_getglobal(L, func_name);
  lua_push_data_dict(L, &dict);
  lua_pcall(L, 1, 0, 0);

  std::cout << "Data dict constents in c++: " << std::endl;
  for (const auto& field : {"a", "b", "c", "d"}) {
    std::cout << field << ": " << dict.field(field) << std::endl;
  }

  return 0;
}
