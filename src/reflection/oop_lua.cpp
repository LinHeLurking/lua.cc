#include "oop_lua.h"

#include <string>
#include <vector>

#include "../common/logging.h"
#include "boost/describe.hpp"
#include "boost/describe/class.hpp"

class ObjA {
 private:
 public:
  double take_action(const std::string& action, double a,
                     double b) const noexcept {
    if (action == "add") {
      return a + b;
    } else if (action == "sub") {
      return a - b;
    } else if (action == "mul") {
      return a * b;
    } else if (action == "div") {
      return a / b;
    } else {
      logf("Unknown action!");
      return 0.0;
    }
  }

  BOOST_DESCRIBE_CLASS(ObjA, (), (take_action), (), ());
};

class ObjB {
 public:
  int x_;
  std::string y_;

  ObjB(int x, const std::string& y) : x_(x), y_(y) {}

  BOOST_DESCRIBE_CLASS(ObjB, (), (x_, y_), (), ());
};

class ObjC {
 public:
  std::string u_;
  int v_;

  ObjC(const std::string& x, int y) : u_(x), v_(y) {}

  BOOST_DESCRIBE_CLASS(ObjC, (), (u_, v_), (), ());
};

int main(int argc, char** argv) {
  std::vector<std::string> load_files;
  for (int i = 1; i < argc; ++i) {
    load_files.push_back(std::string(argv[i]));
  }
  Lua lua(load_files);

  logf("--------------------------------------------");
  lua.call("greet_person", Lua::IGNORED, "Li Hua", 12, "student");

  logf("--------------------------------------------");
  double sum = -1;
  lua.call("a_plus_b", sum, 1.0, 2.0);
  logf("a + b = %lf", sum);

  logf("--------------------------------------------");
  logf(
      "Expect return 2 values from lua. One is number and the other is "
      "string.");
  std::pair<double, std::string> x;
  lua.call("num_n_str", x);
  logf("%lf, %s", x.first, x.second.c_str());

  logf("--------------------------------------------");
  logf("Call C++ member functions inside lua.");
  lua.register_type<ObjA>();
  ObjA obja;
  lua.call("take_action", Lua::IGNORED, &obja);

  logf("--------------------------------------------");
  logf("Bind C++ public member variables with setter/getter functions.");
  lua.register_type<ObjB>();
  lua.register_type<ObjC>();
  ObjB obj_a(1, "C++ Object A!");
  ObjC obj_b("C++ Object B!", 1);
  lua.call("consume_obj_a", Lua::IGNORED, &obj_a);
  logf("This is obj a: {x_: %d, y_: %s}", obj_a.x_, obj_a.y_.c_str());
  lua.call("consume_obj_b", Lua::IGNORED, &obj_b);
  logf("This is obj b: {u_: %s, v_: %d}", obj_b.u_.c_str(), obj_b.v_);

  return 0;
}
