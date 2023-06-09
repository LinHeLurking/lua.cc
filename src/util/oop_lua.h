#pragma once

#include <libintl.h>

#include <cassert>
#include <cstdlib>
#include <regex>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../common/logging.h"
#include "../util/util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifdef __cplusplus
}
#endif

class Lua {
 private:
  lua_State* lua_;

  inline int protected_call(int nargs, int nresults, int errfunc) {
    int flag = lua_pcall(lua_, nargs, nresults, errfunc);
    if (flag != 0) {
      logf("call error: %s", lua_tostring(lua_, -1));
    }
    return flag;
  }

  template <class T>
  struct ret_helper {
    // In most cases it's 1.
    static constexpr size_t count = 1;

    inline static void extract_res(Lua* lua, T& ret) { ret = lua->pop<T>(); }
  };

  // Nop if lua does not return;
  template <>
  struct ret_helper<void*> {
    static constexpr size_t count = 0;
    inline static void extract_res(Lua* lua, void*& ret) {}
  };

  template <class T, class U>
  struct ret_helper<std::pair<T, U>> {
    // std::pair<?, ?> is actually 2 values.
    static constexpr size_t count = 2;

    inline static void extract_res(Lua* lua, std::pair<T, U>& ret) {
      // Pop in reverse order
      ret.second = lua->pop<U>();
      ret.first = lua->pop<T>();
    }
  };

 public:
  Lua() {
    lua_ = luaL_newstate();
    luaL_openlibs(lua_);
  }

  Lua(const std::vector<std::string>& load_files) : Lua() {
    for (const auto& file : load_files) {
      int flag = luaL_dofile(lua_, file.c_str());
      if (flag) {
        logf("Error when loading lua files: %s", file.c_str());
        throw std::runtime_error("Lua load fail");
      }
    }
  }

  inline static lua_detail::IgnoredRetT IGNORED = 0;

  template <class T>
  inline void push(T x) noexcept {
    lua_detail::push(lua_, x);
  }

  template <class T>
  inline void register_type() {
    lua_detail::register_type<T>(lua_);
  }

  template <class K, class V>
  inline void register_map_type() {
    lua_detail::register_map_type<K, V>(lua_);
  }

  template <class T>
  inline T pop() noexcept {
    return lua_detail::pop<T>(lua_);
  }

  template <class Ret, class... Arg>
  int call(const char* lua_func_name, Ret& ret, Arg&&... arg) {
    lua_getglobal(lua_, lua_func_name);
    assert(lua_isfunction(lua_, -1));
    // Push all arguments
    (push(arg), ...);
    // Count the number of arguments
    constexpr int nargs = int(sizeof...(Arg));
    // Count the number of return values.
    constexpr int nresults = int(ret_helper<Ret>::count);
    int flag = protected_call(nargs, nresults, 0);
    ret_helper<Ret>::extract_res(this, ret);
    return flag;
  }

  template <class Ret, class... Arg>
  int call_in_table(const char* table, const char* lua_func_name, Ret& ret,
                    Arg&&... arg) {
    lua_getglobal(lua_, table);
    assert(lua_istable(lua_, -1));
    lua_getfield(lua_, -1, lua_func_name);
    // Push all arguments
    (push(arg), ...);
    // Count the number of arguments
    constexpr int nargs = int(sizeof...(Arg));
    // Count the number of return values.
    constexpr int nresults = int(ret_helper<Ret>::count);
    int flag = protected_call(nargs, nresults, 0);
    ret_helper<Ret>::extract_res(this, ret);
    assert(lua_istable(lua_, -1));
    lua_pop(lua_, 1);
    return flag;
  }
};
