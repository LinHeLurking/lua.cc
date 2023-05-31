#include <algorithm>
#include <cassert>
#include <string>

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

class Person {
 protected:
  int age_;
  std::string name_;
  std::string career_;

 public:
  Person(int age, const std::string& name, const std::string& career)
      : age_(age), name_(name), career_(career) {}

  virtual void greet() const {
    logf("Hello I'm %s and I'm %d years old! I'm a %s!", name_.c_str(), age_,
         career_.c_str());
  }

  virtual ~Person() = default;
};

class Student : public Person {
 public:
  Student(int age, const std::string& name) : Person(age, name, "student") {}

  void greet() const override {
    Person::greet();
    logf("I love study!");
  }
};

class Driver : public Person {
 public:
  Driver(int age, const std::string& name) : Person(age, name, "driver") {}

  void greet() const override {
    Person::greet();
    logf("I love driving!");
  }
};

int lua_person_greet(lua_State* L) {
  auto pptr =
      static_cast<Person**>(luaL_checkudata(L, 1, "PersonPtrMetatable"));
  if (pptr == nullptr) {
    logf("greet nullptr");
    return 0;
  }
  (*pptr)->greet();
  return 0;
}

void lua_register_person(lua_State* L) {
  int flag;
  flag = luaL_newmetatable(L, "PersonPtrMetatable");
  if (!flag) {
    logf("meta table has already been created!");
    return;
  }

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  luaL_Reg person_methods[] = {{"greet", lua_person_greet}, {nullptr, nullptr}};
  luaL_register(L, nullptr, person_methods);
}

void lua_push_person(lua_State* L, Person* person) {
  auto pptr = static_cast<Person**>(lua_newuserdata(L, sizeof(Person*)));
  assert(pptr != nullptr);
  *pptr = person;
  int flag = luaL_newmetatable(L, "PersonPtrMetatable");
  assert(flag == 0);
  lua_setmetatable(L, -2);
}

int main(int argc, const char** argv) {
  if (argc < 3) {
    logf("Usage: <executable> <lua_file> <lua_func>");
    return -1;
  }
  const char* file_name = argv[1];
  const char* func_name = argv[2];

  Person* student = new Student(13, "Li Hua");
  Person* driver = new Driver(26, "Li Hua");

  logf("Call directly in C++:");
  student->greet();
  driver->greet();

  logf("------------------------");
  logf("Call in Lua:");
  
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  
  lua_register_person(L);
  
  luaL_dofile(L, file_name);
  
  int flag;
  
  lua_getglobal(L, func_name);
  lua_push_person(L, student);
  flag = lua_pcall(L, 1, 0, 0);
  if (flag != 0) {
    logf("Call error: %s", lua_tostring(L, -1));
    return -1;
  }
  
  lua_getglobal(L, func_name);
  lua_push_person(L, driver);
  flag = lua_pcall(L, 1, 0, 0);
  if (flag != 0) {
    logf("Call error: %s", lua_tostring(L, -1));
    return -1;
  }

  delete student;
  delete driver;

  return 0;
}
