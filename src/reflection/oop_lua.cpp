#include "oop_lua.h"

#include <string>
#include <vector>

#include "../common/logging.h"
#include "boost/describe.hpp"
#include "boost/describe/class.hpp"

class Obj {
 public:
  int x_;
  std::string y_;

  Obj(int x, const std::string& y) : x_(x), y_(y) {}

  BOOST_DESCRIBE_CLASS(Obj, (), (x_, y_), (), ());
};

int main(int argc, char** argv) {
  std::vector<std::string> load_files;
  for (int i = 1; i < argc; ++i) {
    load_files.push_back(std::string(argv[i]));
  }
  Lua lua(load_files);

  lua.call("greet_person", Lua::IGNORED, "Li Hua", 12, "student");

  double sum = -1;
  lua.call("a_plus_b", sum, 1.0, 2.0);
  logf("a + b = %lf", sum);

  std::pair<double, std::string> x;
  lua.call("num_n_str", x);
  logf("%lf, %s", x.first, x.second.c_str());

  lua.register_type<Obj>();
  Obj obj(1, "C++ Object!");
  lua.call("describe_obj", Lua::IGNORED, &obj);

  return 0;
}
