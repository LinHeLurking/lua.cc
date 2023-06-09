#pragma once

#include <algorithm>
#include <boost/callable_traits/args.hpp>
#include <boost/callable_traits/remove_member_const.hpp>
#include <boost/callable_traits/remove_noexcept.hpp>
#include <boost/callable_traits/return_type.hpp>
#include <boost/core/demangle.hpp>
#include <boost/core/type_name.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>
#include <exception>
#include <functional>
#include <regex>
#include <tuple>
#include <type_traits>
#include <utility>

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

template <class T>
class ClazzMeta {
 public:
  // These variables are static because lambda expressions need them but cannot
  // capture them.
  inline static std::string NAME = boost::core::type_name<T>();
  inline static std::string PROTOTYPE_NAME = NAME + "PtrPrototype";
  inline static std::string METATABLE_NAME = NAME + "PtrMetatable";
  inline static std::unordered_map<std::string, lua_CFunction> METHODS = {},
                                                               GETTERS = {},
                                                               SETTERS = {};
  inline static bool REGISTERED = false;
};

using IgnoredRetT = void*;
inline static IgnoredRetT IGNORED = 0;

// Pushes int/float/double... values into lua stack
template <class T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline void push(lua_State* lua, T x) noexcept {
  if constexpr (std::is_same_v<bool, std::decay_t<T>>) {
    lua_pushboolean(lua, x);
  } else {
    lua_pushnumber(lua, x);
  }
}

// Pushes C style string into lua stack
template <class T,
          typename std::enable_if_t<std::is_same_v<T, const char*>, int> = 0>
inline void push(lua_State* lua, T s) noexcept {
  lua_pushstring(lua, s);
}

// Pushes C++ style string into lua stack. Both `std::string&` and `const
// std::string&` are ok.
template <class T,
          typename std::enable_if_t<
              std::is_same_v<std::string, std::remove_const_t<T>>, int> = 0>
inline void push(lua_State* lua, T& s) noexcept {
  lua_pushstring(lua, s.c_str());
}

// Pushes any boost::describe annotated C++ class into lua stack.
template <class T,
          typename std::enable_if_t<
              boost::describe::has_describe_members<T>::value, int> = 0>
inline void push(lua_State* lua, T* x) {
  auto pptr = static_cast<T**>(lua_newuserdata(lua, sizeof(T*)));
  *pptr = x;
  static std::string meta_table_name = ClazzMeta<T>::METATABLE_NAME;
  luaL_newmetatable(lua, meta_table_name.c_str());
  lua_setmetatable(lua, -2);
}

template <typename T, typename U = void>
struct is_mappish_impl : std::false_type {};

template <typename T>
struct is_mappish_impl<
    T, std::void_t<typename T::key_type, typename T::mapped_type,
                   decltype(std::declval<T&>()
                                [std::declval<const typename T::key_type&>()])>>
    : std::true_type {};

template <typename T>
struct is_mappish : is_mappish_impl<T>::type {};

// Pushes map type
template <class T, typename std::enable_if_t<is_mappish<T>::value, int> = 0>
inline void push(lua_State* lua, T* x) {
  auto pptr = static_cast<T**>(lua_newuserdata(lua, sizeof(T*)));
  *pptr = x;
  static std::string meta_table_name = ClazzMeta<T>::METATABLE_NAME;
  luaL_newmetatable(lua, meta_table_name.c_str());
  lua_setmetatable(lua, -2);
}

// Pops int/float/double.. values from lua stack.
template <class T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline T pop(lua_State* lua) noexcept {
  if constexpr (std::is_same_v<bool, std::decay_t<T>>) {
    T ret = lua_toboolean(lua, -1);
    lua_pop(lua, 1);
    return ret;
  } else {
    T ret = lua_tonumber(lua, -1);
    lua_pop(lua, 1);
    return ret;
  }
}

// Pops C++ style string from lua stack.
// Due to potential memory free by lua GC, this function copy all string data
// out.
template <class T,
          typename std::enable_if_t<std::is_same_v<std::string, T>, int> = 0>
inline T pop(lua_State* lua) noexcept {
  const char* c_str = lua_tostring(lua, -1);
  std::string ret = c_str;
  lua_pop(lua, 1);
  return ret;
}

// Pops user registered data type pointers. Here T is some pointer type.
template <class T,
          typename std::enable_if_t<boost::describe::has_describe_members<
                                        std::remove_pointer_t<T>>::value,
                                    int> = 0>
inline auto pop(lua_State* lua) noexcept {
  using RawT = typename std::remove_pointer_t<T>;
  auto pptr = static_cast<RawT**>(
      luaL_checkudata(lua, 1, ClazzMeta<RawT>::METATABLE_NAME.c_str()));
  lua_pop(lua, 1);
  return *pptr;
}

template <class T>
inline int register_prototype(lua_State* lua) {
  int flag;
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
  prototype_exec_str =
      std::regex_replace(prototype_exec_str, std::regex("__prototype__"),
                         ClazzMeta<T>::PROTOTYPE_NAME);
  flag = luaL_dostring(lua, prototype_exec_str.c_str());
  if (flag != 0) {
    logf("Do string error: %s", lua_tostring(lua, -1));
    return flag;
  }
  // Prepare luaL_Reg
  std::vector<luaL_Reg> methods, getters, setters;
  for (const auto& [k, v] : ClazzMeta<T>::METHODS) {
    methods.push_back({k.c_str(), v});
  }
  methods.push_back({nullptr, nullptr});
  for (const auto& [k, v] : ClazzMeta<T>::GETTERS) {
    getters.push_back({k.c_str(), v});
  }
  getters.push_back({nullptr, nullptr});
  for (const auto& [k, v] : ClazzMeta<T>::SETTERS) {
    setters.push_back({k.c_str(), v});
  }
  setters.push_back({nullptr, nullptr});

  // Query prototype table and push it to stack
  lua_getglobal(lua, ClazzMeta<T>::PROTOTYPE_NAME.c_str());  // will push

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

  // Pop prototype table
  lua_pop(lua, 1);

  return 0;
}

template <class K, class V>
int lua_map_at(lua_State* lua) {
  using T = std::unordered_map<K, V>;
  auto pptr = static_cast<T**>(
      luaL_checkudata(lua, -2, ClazzMeta<T>::METATABLE_NAME.c_str()));
  K key = pop<K>(lua);
  V value = (*pptr)->at(key);
  push(lua, value);
  return 1;
}

template <class K, class V>
inline int register_map_type(lua_State* lua) noexcept {
  using T = std::unordered_map<K, V>;
  ClazzMeta<T>::PROTOTYPE_NAME = std::regex_replace(
      ClazzMeta<T>::PROTOTYPE_NAME, std::regex(":|<|>| |,"), "_");
  ClazzMeta<T>::METATABLE_NAME = std::regex_replace(
      ClazzMeta<T>::METATABLE_NAME, std::regex(":|<|>| |,"), "_");
  logf("Map prototype name: %s", ClazzMeta<T>::PROTOTYPE_NAME.c_str());
  logf("Map metatbale name: %s", ClazzMeta<T>::METATABLE_NAME.c_str());

  int flag;
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
  prototype_exec_str =
      std::regex_replace(prototype_exec_str, std::regex("__prototype__"),
                         ClazzMeta<T>::PROTOTYPE_NAME);
  flag = luaL_dostring(lua, prototype_exec_str.c_str());
  if (flag != 0) {
    logf("Do string error: %s", lua_tostring(lua, -1));
    return flag;
  }
  // Prepare luaL_Reg
  std::vector<luaL_Reg> methods, getters, setters;
  methods.push_back({"at", lua_map_at<K, V>});
  methods.push_back({nullptr, nullptr});
  getters.push_back({nullptr, nullptr});
  setters.push_back({nullptr, nullptr});

  // Query prototype table and push it to stack
  lua_getglobal(lua, ClazzMeta<T>::PROTOTYPE_NAME.c_str());  // will push

  // Extract field table `__methods`
  lua_getfield(lua, -1, "__methods");
  assert(lua_istable(lua, -1));
  // Register methods into `__methods` table.
  luaL_register(lua, nullptr, methods.data());
  // Pop table.
  lua_pop(lua, 1);

  flag = luaL_newmetatable(lua,
                           ClazzMeta<T>::METATABLE_NAME.c_str());  // will push
  if (!flag) {
    logf("Meta table has already been created!");
    return 1;
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

  return 0;
}

template <class T>
struct is_str_ref {
  // Is std::string after decaying. And is a reference.
  inline static constexpr bool value =
      std::is_same_v<std::string, std::decay_t<T>> && std::is_reference_v<T>;
};

template <class T>
inline constexpr bool is_str_ref_v = is_str_ref<T>::value;

template <class T>
inline constexpr bool is_registered_type_pointer_v =
    std::is_pointer_v<T> &&
    boost::describe::has_describe_members<std::decay_t<T>>::value;

template <class T>
inline constexpr bool is_registered_type_ref_v =
    std::is_reference_v<T> &&
    boost::describe::has_describe_members<std::decay_t<T>>::value;

template <class ArgTuple, size_t I>
inline auto pop_args_impl(lua_State* lua, int index) noexcept {
  constexpr size_t N = std::tuple_size_v<ArgTuple>;
  using T = std::tuple_element_t<I, ArgTuple>;
  if constexpr (std::is_arithmetic_v<T>) {
    assert(lua_isnumber(lua, index));
    // logf("pop number at %d: %lf", index, lua_tonumber(lua, index));
    return lua_tonumber(lua, index);
  } else if constexpr (std::is_same_v<std::string, T>) {
    assert(lua_isstring(lua, index));
    // logf("pop string at %d: %s", index, lua_tostring(lua, index));
    return std::string(lua_tostring(lua, index));
  } else if constexpr (is_registered_type_ref_v<T>) {
    using RawT = typename std::decay_t<T>;
    auto pptr = static_cast<RawT**>(
        luaL_checkudata(lua, index, ClazzMeta<RawT>::METATABLE_NAME.c_str()));
    // logf("pop udata at %d: %p", index, *pptr);
    return std::ref(**pptr);
  } else {
    // Always fail check
    static_assert(!std::is_same_v<T, T>, "Not supported type!");
  }
}

template <class ArgTuple, size_t... I>
inline auto pop_args(lua_State* lua, std::index_sequence<I...>) noexcept {
  constexpr size_t N = sizeof...(I);
  // pos(-N-1) is `*this`, pos(-N) is the first argument and so on.
  auto args = std::tuple(pop_args_impl<ArgTuple, I>(lua, -int(N) + int(I))...);
  // logf("Popping %d args", sizeof...(I));
  lua_pop(lua, int(N));
  return args;
}

template <class T>
inline void extract_methods() {
  using namespace boost::describe;
  using namespace boost::mp11;
  using namespace boost::callable_traits;

  // methods
  using M_FUNCS = describe_members<T, mod_public | mod_function>;
  mp_for_each<M_FUNCS>([](auto&& func) {
    // These variables are static because lambda expressions need them but
    // cannot capture them.
    static std::string meta_table_name =
        boost::core::type_name<T>() + "PtrMetatable";
    static const char* fn_name = func.name;
    static auto fn_ptr = func.pointer;

    lua_CFunction method = [](lua_State* lua) -> int {
      // Raw func type after remove membership. It might have `const` or
      // `noexcept` modifier. Signature sample: int f(int) const noexcept;
      using FuncTRaw = typename member_pointer<decltype(func.pointer)>::type;
      // Remove const/noexcept
      using FuncT = remove_noexcept_t<remove_member_const_t<FuncTRaw>>;
      // Return type of function.
      using RetT = return_type_t<FuncT>;
      // A std::tuple<...> of types of all arguments.
      using ArgTupleTRaw = args_t<FuncT>;
      // Remove ref of std::string arguments because lua doesn't guarantee const
      // char * is valid after it is popped from stack.
      using ArgTupleTNoStrRef =
          mp_transform_if<is_str_ref, std::remove_reference_t, ArgTupleTRaw>;
      // Remove const/volatile modifilers because Lua has no such syntax.
      using ArgTupleTNoCV = mp_transform<std::remove_cv_t, ArgTupleTNoStrRef>;
      using ArgTupleT = ArgTupleTNoCV;

      constexpr size_t N_ARG = std::tuple_size_v<ArgTupleT>;
      auto pptr =
          static_cast<T**>(luaL_checkudata(lua, 1, meta_table_name.c_str()));
      ArgTupleT f_args =
          pop_args<ArgTupleT>(lua, std::make_index_sequence<N_ARG>());
      // Call member function with tuple, prepending reference of object ptr.
      if constexpr (std::is_same_v<RetT, void>) {
        // Ignore void return value.
        std::apply(fn_ptr,
                   std::tuple_cat(std::make_tuple(std::ref(**pptr)), f_args));
        return 0;
      } else {
        RetT res = std::apply(
            fn_ptr, std::tuple_cat(std::make_tuple(std::ref(**pptr)), f_args));
        push<RetT>(lua, res);
        return 1;
      }
    };
    ClazzMeta<T>::METHODS[fn_name] = method;
  });
}

template <class T>
inline void extract_getter_setter() {
  using namespace boost::describe;
  using namespace boost::mp11;
  using namespace boost::callable_traits;
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
          luaL_checkudata(lua, 1, ClazzMeta<T>::METATABLE_NAME.c_str()));
      lua_detail::push<MemberT>(lua, (*pptr)->*pointer);
      return 1;
    };
    lua_CFunction setter = [](lua_State* lua) -> int {
      auto pptr = static_cast<T**>(
          luaL_checkudata(lua, 1, ClazzMeta<T>::METATABLE_NAME.c_str()));
      auto value = lua_detail::pop<MemberT>(lua);
      (*pptr)->*pointer = value;
      return 0;
    };

    ClazzMeta<T>::SETTERS[name] = setter;
    ClazzMeta<T>::GETTERS[name] = getter;
  });
}

template <class T>
inline int register_metatable(lua_State* lua) {
  // Query prototype table and push it to stack
  lua_getglobal(lua, ClazzMeta<T>::PROTOTYPE_NAME.c_str());  // will push

  int flag =
      luaL_newmetatable(lua,
                        ClazzMeta<T>::METATABLE_NAME.c_str());  // will push
  if (!flag) {
    logf("Meta table has already been created!");
    return 1;
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
  return 0;
}

template <class T>
inline void register_type(lua_State* lua) {
  if (ClazzMeta<T>::REGISTERED) return;

  extract_methods<T>();

  extract_getter_setter<T>();

  int flag = register_prototype<T>(lua);
  if (flag != 0) {
    logf("Prototype register error: %s", lua_tostring(lua, -1));
    return;
  }

  register_metatable<T>(lua);
  if (flag != 0) {
    logf("Metatable register error: %s", lua_tostring(lua, -1));
    return;
  }

  ClazzMeta<T>::REGISTERED = true;
}
}  // namespace lua_detail
