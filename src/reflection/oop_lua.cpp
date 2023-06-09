#include "../util/oop_lua.h"

#include <string>
#include <vector>

#include "../common/logging.h"
#include "boost/describe.hpp"
#include "boost/describe/class.hpp"

class TestMethod {
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

  BOOST_DESCRIBE_CLASS(TestMethod, (), (take_action), (), ());
};

class TestNotSameDataTypeX {
 public:
  int x_;
  std::string y_;

  TestNotSameDataTypeX(int x, const std::string& y) : x_(x), y_(y) {}

  // static void rewrite_str(ObjB& another, const std::string& str) {
  //   another.y_ = str;
  // }

  BOOST_DESCRIBE_CLASS(TestNotSameDataTypeX, (), (x_, y_), (), ());
};

class TestRefArg {
 public:
  std::string str_;

  TestRefArg(const std::string& s) : str_(s) {}

  void overwrite_append(TestRefArg& another, const std::string& tail) {
    str_ = another.str_ + tail;
  }

  BOOST_DESCRIBE_CLASS(TestRefArg, (), (str_, overwrite_append), (), ());
};

class TestStaticMethod {
 private:
  std::string str_;

 public:
  TestStaticMethod(const std::string& s) : str_(s) {}

  static void overwrite_str(TestStaticMethod& another, const std::string& s) {
    another.str_ = s;
  }
};

class TestNotSameDataTypeY {
 public:
  std::string u_;
  int v_;

  TestNotSameDataTypeY(const std::string& u, int v) : u_(u), v_(v) {}

  BOOST_DESCRIBE_CLASS(TestNotSameDataTypeY, (), (u_, v_), (), ());
};

int main(int argc, char** argv) {
  std::vector<std::string> load_files;
  for (int i = 1; i < argc; ++i) {
    load_files.push_back(std::string(argv[i]));
  }
  Lua lua(load_files);
  lua.register_type<TestMethod>();
  lua.register_type<TestNotSameDataTypeX>();
  lua.register_type<TestNotSameDataTypeY>();
  lua.register_type<TestRefArg>();

  {
    logf("--------------------------------------------");
    lua.call("greet_person", Lua::IGNORED, "Li Hua", 12, "student");
  }

  {
    logf("--------------------------------------------");
    double sum = -1;
    lua.call("a_plus_b", sum, 1.0, 2.0);
    logf("a + b : %lf", sum);
  }

  {
    logf("--------------------------------------------");
    logf("Calling some function inside lua table(namespace)");
    bool is_ge = true;
    lua.call_in_table("lib", "a_and_b", is_ge, true, false);
    logf("a && b : %s", is_ge ? "true" : "false");
  }

  {
    logf("--------------------------------------------");
    logf(
        "Expect return 2 values from lua. One is number and the other is "
        "string.");
    std::pair<double, std::string> x;
    lua.call("num_n_str", x);
    logf("%lf, %s", x.first, x.second.c_str());
  }

  {
    logf("--------------------------------------------");
    logf("Call C++ member functions inside lua.");
    TestMethod obja;
    lua.call("take_action", Lua::IGNORED, &obja);
  }

  {
    logf("--------------------------------------------");
    logf("Calling C++ member function with reference argument.");
    TestRefArg x("x!"), y("y!");
    lua.call("overwrite_append", Lua::IGNORED, &x, &y);
    logf("This is x: { str_: %s}", x.str_.c_str());
  }

  {
    logf("--------------------------------------------");
    logf("Bind C++ public member variables with setter/getter functions.");
    TestNotSameDataTypeX obj_a(1, "C++ Object A!");
    TestNotSameDataTypeY obj_b("C++ Object B!", 1);
    lua.call("consume_obj_a", Lua::IGNORED, &obj_a);
    logf("This is obj a: {x_: %d, y_: %s}", obj_a.x_, obj_a.y_.c_str());
    lua.call("consume_obj_b", Lua::IGNORED, &obj_b);
    logf("This is obj b: {u_: %s, v_: %d}", obj_b.u_.c_str(), obj_b.v_);
  }

  return 0;
}
