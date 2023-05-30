#include "../common/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "../../luajit/src/lauxlib.h"
#include "../../luajit/src/lua.h"
#include "../../luajit/src/lualib.h"

#ifdef __cplusplus
}
#endif

class Student {
 public:
  int age_;
  std::string name_;

  void greet() const noexcept {
    logf("Hello! I'm %s and I'm %d years old!", name_.c_str(), age_);
  }
};

int main(int argc, const char** argv) {
  //
  return 0;
}
