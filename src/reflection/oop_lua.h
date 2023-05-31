#pragma once

#include <cassert>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

#include "../common/logging.h"

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

  using IgnoredRetT = void*;
  inline static void* IGNORED = 0;

  // Pushes int/float/double... values into lua stack
  template <class T,
            typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
  inline void push(T x) noexcept {
    lua_pushnumber(lua_, x);
  }

  // Pushes C style string into lua stack
  template <class T,
            typename std::enable_if_t<std::is_same_v<T, const char*>, int> = 1>
  inline void push(T s) noexcept {
    lua_pushstring(lua_, s);
  }

  // Pushes C++ style string into lua stack. Both `std::string&` and `const
  // std::string&` are ok. Does not receive `std::string`.
  template <class T,
            typename std::enable_if_t<
                std::is_same_v<std::string&, std::remove_const_t<T>>, int> = 2>
  inline void push(T s) noexcept {
    lua_pushstring(lua_, s.c_str());
  }

  // Pops int/float/double.. values from lua stack.
  template <class T,
            typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
  inline T pop() noexcept {
    T ret = lua_tonumber(lua_, -1);
    lua_pop(lua_, 1);
    return ret;
  }

  // Pops C++ style string from lua stack.
  // Due to potential memory free by lua GC, this function copy all string data
  // out.
  template <class T,
            typename std::enable_if_t<std::is_same_v<std::string, T>, int> = 1>
  inline T pop() noexcept {
    const char* c_str = lua_tostring(lua_, -1);
    std::string ret = c_str;
    lua_pop(lua_, 1);
    return ret;
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
};
