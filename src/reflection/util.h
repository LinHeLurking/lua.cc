#pragma once

#include <boost/callable_traits/remove_member_cv.hpp>
#include <boost/core/demangle.hpp>
#include <boost/core/type_name.hpp>
#include <functional>
#include <regex>
#include <type_traits>

#include "../common/logging.h"
#include "boost/core/type_name.hpp"
#include "boost/describe/members.hpp"
#include "boost/describe/modifiers.hpp"
#include "boost/mp11/algorithm.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifdef __cplusplus
}
#endif

template <class T>
struct member_pointer {};

template <class T, class C>
struct member_pointer<T C::*const> {
  using type = T;
};

template <class T, class C>
struct member_pointer<T C::*> {
  using type = T;
};

template <class Ptr>
const char* name_from_ptr(Ptr ptr) {
  using T = typename member_pointer<Ptr>::type;
  static auto name = boost::core::type_name<T>();
  return name.c_str();
}

class ClazzLuaMeta {
 public:
  std::unordered_map<std::string, lua_CFunction> methods_,
      getters_, setters_;
};

namespace lua_detail {

using IgnoredRetT = void*;
inline static IgnoredRetT IGNORED = 0;

// Pushes int/float/double... values into lua stack
template <class T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline void push(lua_State* lua, T x) noexcept {
  lua_pushnumber(lua, x);
}

// Pushes C style string into lua stack
template <class T,
          typename std::enable_if_t<std::is_same_v<T, const char*>, int> = 1>
inline void push(lua_State* lua, T s) noexcept {
  lua_pushstring(lua, s);
}

// Pushes C++ style string into lua stack. Both `std::string&` and `const
// std::string&` are ok. Does not receive `std::string`.
template <class T,
          typename std::enable_if_t<
              std::is_same_v<std::string, std::remove_const_t<T>>, int> = 2>
inline void push(lua_State* lua, T& s) noexcept {
  lua_pushstring(lua, s.c_str());
}

// Pushes any boost::describe annotated C++ class into lua stack.
template <class T,
          typename std::enable_if_t<
              boost::describe::has_describe_members<T>::value, int> = 3>
inline void push(lua_State* lua, T* x) {
  auto pptr = static_cast<T**>(lua_newuserdata(lua, sizeof(T*)));
  *pptr = x;
  static std::string meta_table_name =
      boost::core::type_name<T>() + "PtrMetatable";
  luaL_newmetatable(lua, meta_table_name.c_str());
  lua_setmetatable(lua, -2);
}

// Pops int/float/double.. values from lua stack.
template <class T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline T pop(lua_State* lua) noexcept {
  T ret = lua_tonumber(lua, -1);
  lua_pop(lua, 1);
  return ret;
}

// Pops C++ style string from lua stack.
// Due to potential memory free by lua GC, this function copy all string data
// out.
template <class T,
          typename std::enable_if_t<std::is_same_v<std::string, T>, int> = 1>
inline T pop(lua_State* lua) noexcept {
  const char* c_str = lua_tostring(lua, -1);
  std::string ret = c_str;
  lua_pop(lua, 1);
  return ret;
}

template <class T>
int __xxx__(lua_State *l) {
  return 0;
}

template <class T>
inline void register_type(lua_State* lua) {
  int flag;
  auto name = boost::core::type_name<T>();
  auto type_ptr_prototype_name = name + "PtrPrototype";
  std::string prototype_exec_str =
      "\
      __prototype__ = { \n \
        __methods = {}, \n \
        __getters = {}, \n \
        __setters = {}, \n \
      }\n \
      function __prototype__.__impl_index(self, key) \n \
        if __prototype__.__methods[key] ~= nil then \n \
          return __prototype__.__methods[key] \n \
        end \n \
        if __prototype__.__getters[key] ~= nil then \n \
          return __prototype__.__getters[key](self) \n \
        end \n \
        return nil \n \
      end \n \
      function __prototype__.__impl_newindex(self, key, value) \n \
        if __prototype__.__setters[key] ~= nil then \n \
          __prototype__.__setters[key](self, value) \n \
        end \n \
      end \n \
      ";
  // Replace `__prototype__` as type ptr name
  prototype_exec_str = std::regex_replace(
      prototype_exec_str, std::regex("__prototype__"), type_ptr_prototype_name);
  flag = luaL_dostring(lua, prototype_exec_str.c_str());
  if (flag != 0) {
    logf("Prototype register error: %s", lua_tostring(lua, -1));
    return;
  }

  using namespace boost::describe;

  static ClazzLuaMeta clazz_meta;

  // methods
  // TODO:

  // Getters & setters
  using M_VARS = describe_members<T, mod_public>;

  boost::mp11::mp_for_each<M_VARS>([](auto&& member) {
    static std::string meta_table_name = boost::core::type_name<T>() + "PtrMetatable";

    std::string name = member.name;
    static auto pointer = member.pointer;
    // Outside lambda capture controls access, so here it can capture all
    // without problem.
    lua_CFunction getter = [](lua_State* lua) -> int {
      auto pptr =
          static_cast<T**>(luaL_checkudata(lua, 1, meta_table_name.c_str()));
      lua_detail::push(lua, (**pptr).*pointer);
      return 1;
    };
    lua_CFunction setter = [](lua_State* lua) -> int {
      using member_type =
          typename member_pointer<decltype(member.pointer)>::type;
      // Get and pop the first arg (obj ptr)
      auto pptr =
          static_cast<T**>(luaL_checkudata(lua, 1, meta_table_name.c_str()));
      lua_pop(lua, 1);
      // Get and pop the second arg (new value)
      auto value = lua_detail::pop<member_type>(lua);
      // Set value through member pointer
      (**pptr).*pointer = value;
      return 0;
    };

    clazz_meta.setters_[name] = setter;
    clazz_meta.getters_[name] = getter;
  });

  std::vector<luaL_Reg> methods, getters, setters;
  for (const auto& [k, v] : clazz_meta.methods_) {
    methods.push_back({k.c_str(), v});
  }
  methods.push_back({nullptr, nullptr});
  for (const auto& [k, v] : clazz_meta.getters_) {
    getters.push_back({k.c_str(), v});
  }
  getters.push_back({nullptr, nullptr});
  for (const auto& [k, v] : clazz_meta.setters_) {
    setters.push_back({k.c_str(), v});
  }
  setters.push_back({nullptr, nullptr});

  // Query prototype table and push it to stack
  lua_getglobal(lua, type_ptr_prototype_name.c_str());  // will push

  // Extract field table `__methods`
  lua_getfield(lua, -1, "__methods");
  assert(lua_istable(lua, -1));
  // Register methods into `__methods` table.
  luaL_register(lua, nullptr, methods.data());
  // Pop table.
  lua_pop(lua, 1);

  // Extract field table `__getters`
  lua_getfield(lua, -1, "__getters");
  assert(lua_istable(lua, -1));
  // Register getters into `__getters` table.
  luaL_register(lua, nullptr, getters.data());
  // Pop table.
  lua_pop(lua, 1);

  // Extract field table `__setters`
  lua_getfield(lua, -1, "__setters");
  assert(lua_istable(lua, -1));
  // Register setters into `__setters` table.
  luaL_register(lua, nullptr, setters.data());
  // Pop table
  lua_pop(lua, 1);

  flag = luaL_newmetatable(lua, "StudentPtrMetatable");  // will push
  if (!flag) {
    logf("Meta table has already been created!");
    return;
  }

  // Now stack pos top (-1) is meta-table.
  // Next one (-2) is prototype table.
  // Extract function `__impl_index`
  lua_getfield(lua, -2, "__impl_index");  // will push
  assert(lua_isfunction(lua, -1));
  lua_getfield(lua, -3, "__impl_newindex");  // will push

  // -1: `__impl_newindex`
  // -2: `__impl_index`
  // -3: meta-table
  // -4: prototype table

  lua_setfield(lua, -3, "__newindex");  // will pop
  lua_setfield(lua, -2, "__index");     // will pop
  // Clear stack
  lua_pop(lua, 2);
}

}  // namespace lua_detail
