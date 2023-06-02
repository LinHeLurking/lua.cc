#include <boost/describe.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/algorithm.hpp>
#include <iostream>
#include <type_traits>

#include "../common/logging.h"
#include "../util/util.h"

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

int main(int argc, char** argv) {
  logf("Information about class `Person`:");
  logf("---------------------------------");
  logf("Public member variables:");

  using namespace boost::describe;
  using namespace boost::mp11;
  using namespace lua_detail;

  using M_VAR = describe_members<Person, mod_public>;
  mp_for_each<M_VAR>(
      [](auto&& D) { logf("%s\t[%s]", D.name, name_from_ptr(D.pointer)); });

  logf("---------------------------------");
  logf("Public member functions:");
  using M_FUNC = describe_members<Person, mod_public | mod_function>;
  mp_for_each<M_FUNC>(
      [](auto&& D) { logf("%s\t[%s]", D.name, name_from_ptr(D.pointer)); });

  return 0;
}
