#pragma once

#include <boost/callable_traits/args.hpp>
#include <boost/callable_traits/remove_member_const.hpp>
#include <boost/callable_traits/remove_noexcept.hpp>
#include <boost/callable_traits/return_type.hpp>
#include <boost/core/demangle.hpp>
#include <boost/core/type_name.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/algorithm.hpp>
#include <functional>
#include <regex>
#include <tuple>
#include <type_traits>

#include "../common/logging.h"
#include "boost/mp11/detail/mp_with_index.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifdef __cplusplus
}
#endif

namespace lua_detail {

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

class ClazzMeta {
 public:
  std::unordered_map<std::string, lua_CFunction> methods_, getters_, setters_;
};

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
inline void register_type(lua_State* lua) {
  int flag;
  // These variables are static because lambda expressions need them but cannot
  // capture them.
  static auto name = boost::core::type_name<T>();
  static auto type_prototype_name = name + "PtrPrototype";
  static auto type_metatable_name = name + "PtrMetatable";

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
      prototype_exec_str, std::regex("__prototype__"), type_prototype_name);
  flag = luaL_dostring(lua, prototype_exec_str.c_str());
  if (flag != 0) {
    logf("Prototype register error: %s", lua_tostring(lua, -1));
    return;
  }

  using namespace boost::describe;
  using namespace boost::mp11;
  using namespace boost::callable_traits;

  static ClazzMeta clazz_meta;

  // methods
  using M_FUNCS = describe_members<T, mod_public | mod_function>;
  mp_for_each<M_FUNCS>([lua](auto&& func) {
    // These variables are static because lambda expressions need them but
    // cannot capture them.
    static std::string meta_table_name =
        boost::core::type_name<T>() + "PtrMetatable";
    static const char* fn_name = func.name;
    static const auto fn_ptr = func.pointer;

    lua_CFunction method = [](lua_State* lua) -> int {
      // Raw func type. It might have `const` or `noexcept` modifier.
      // Signature sample: int f(int) const noexcept;
      using FuncTRaw = typename member_pointer<decltype(fn_ptr)>::type;
      // Remove const/noexcept
      using FuncT = remove_noexcept_t<remove_member_const_t<FuncTRaw>>;
      // Return type of function.
      using RetT = return_type_t<FuncT>;
      // A std::tuple<...> of types of all arguments.
      using ArgTupleTRaw = args_t<FuncT>;
      // Remove ref/const/volatile modifiers. All values are copied from stack.
      using ArgTupleT =
          mp_transform<std::remove_cv_t,
                       mp_transform<std::remove_reference_t, ArgTupleTRaw>>;

      constexpr size_t N_ARG = std::tuple_size_v<ArgTupleT>;
      ArgTupleT f_args;
      auto pptr =
          static_cast<T**>(luaL_checkudata(lua, 1, meta_table_name.c_str()));
      // Extract all arguments into tuple in reverse order.
      mp_for_each<mp_reverse<mp_iota_c<N_ARG>>>([lua, &f_args](auto I) {
        // I is instance of std::integral_constant of size_t
        using CurArgT = std::remove_reference_t<decltype(std::get<I>(f_args))>;
        std::get<I>(f_args) = pop<CurArgT>(lua);
      });
      // Call member function with tuple, prepending reference of object ptr.
      RetT res = std::apply(
          fn_ptr, std::tuple_cat(std::make_tuple(std::ref(**pptr)), f_args));
      push<RetT>(lua, res);
      return 1;
    };
    clazz_meta.methods_[fn_name] = method;
  });

  // Getters & setters
  using M_VARS = describe_members<T, mod_public>;

  mp_for_each<M_VARS>([](auto&& member) {
    // These variables are static because lambda expressions need them but
    // cannot capture them.
    static std::string meta_table_name =
        boost::core::type_name<T>() + "PtrMetatable";
    static const char* name = member.name;
    static auto pointer = member.pointer;
    using MemberT = typename member_pointer<decltype(pointer)>::type;
    // Only lambda without capture can be correctly converted into
    // C style function pointer, which is lua acceptable lua_CFunction.
    lua_CFunction getter = [](lua_State* lua) -> int {
      auto pptr = static_cast<T**>(
          luaL_checkudata(lua, 1, type_metatable_name.c_str()));
      lua_detail::push<MemberT>(lua, (*pptr)->*pointer);
      return 1;
    };
    lua_CFunction setter = [](lua_State* lua) -> int {
      auto pptr = static_cast<T**>(
          luaL_checkudata(lua, 1, type_metatable_name.c_str()));
      auto value = lua_detail::pop<MemberT>(lua);
      (*pptr)->*pointer = value;
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
  lua_getglobal(lua, type_prototype_name.c_str());  // will push

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

  flag = luaL_newmetatable(lua, type_metatable_name.c_str());  // will push
  if (!flag) {
    logf("Meta table has already been created!");
    return;
  }
  assert(lua_istable(lua, -1));

  // Now stack pos top (-1) is meta-table.
  // Next one (-2) is prototype table.
  // Extract function `__impl_index`
  lua_getfield(lua, -2, "__impl_index");  // will push
  assert(lua_isfunction(lua, -1));
  lua_getfield(lua, -3, "__impl_newindex");  // will push
  assert(lua_isfunction(lua, -1));

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
