#include <boost/callable_traits/remove_member_cv.hpp>
#include <boost/core/demangle.hpp>
#include <boost/describe.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/algorithm.hpp>
#include <iostream>
#include <type_traits>

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

class Person {
 public:
  int age_;
  std::string name_;
  std::string career_;

  Person(int age, const std::string& name, const std::string& career)
      : age_(age), name_(name), career_(career) {}

  void greet() const noexcept {
    logf("Hello I'm %s and I'm %d years old! I'm a %s!", name_.c_str(), age_,
         career_.c_str());
  }

  void aging(int year) noexcept { age_ += year; }

  // Boost reflection declaration
  BOOST_DESCRIBE_CLASS(Person, (), (age_, name_, career_, greet, aging), (),
                       ());
};

class Lua {
 private:
 public:
  Lua() {}
};

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
std::enable_if_t<!std::is_member_function_pointer_v<Ptr>, const char*>
name_from_ptr(Ptr ptr) {
  using T = typename member_pointer<Ptr>::type;
  static auto name = boost::core::demangle(typeid(T).name());
  return name.c_str();
}

template <class Ptr>
std::enable_if_t<std::is_member_function_pointer_v<Ptr>, const char*>
name_from_ptr(Ptr ptr) {
  using T = typename member_pointer<Ptr>::type;
  using boost::callable_traits::remove_member_cv_t;
  static auto name =
      boost::core::demangle(typeid(remove_member_cv_t<T>).name());
  return name.c_str();
}

int main(int argc, char** argv) {
  logf("Information about class `Person`:");
  logf("---------------------------------");
  logf("Public member variables:");

  using namespace boost::describe;
  using namespace boost::mp11;

  using M_VAR = describe_members<Person, mod_public>;
  mp_for_each<M_VAR>(
      [](auto&& D) { logf("%s(%s)", D.name, name_from_ptr(D.pointer)); });

  logf("---------------------------------");
  logf("Public member functions (const/noexcept ignored):");
  using M_FUNC = describe_members<Person, mod_public | mod_function>;
  mp_for_each<M_FUNC>(
      [](auto&& D) { logf("%s(%s)", D.name, name_from_ptr(D.pointer)); });

  return 0;
}
