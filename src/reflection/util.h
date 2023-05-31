#pragma once
#include <boost/callable_traits/remove_member_cv.hpp>
#include <boost/core/demangle.hpp>
#include <boost/core/type_name.hpp>
#include <type_traits>

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
const char *name_from_ptr(Ptr ptr) {
  using T = typename member_pointer<Ptr>::type;
  static auto name = boost::core::type_name<T>();
  return name.c_str();
}
