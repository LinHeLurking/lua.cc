#include <cassert>

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
  int age_ = -1;
  std::string name_ = "???";

  Student() = default;

  void greet() const noexcept {
    logf("Hello! I'm %s and I'm %d years old!", this->name_.c_str(),
         this->age_);
  }
};

int lua_student_greet(lua_State* L) {
  auto pptr =
      static_cast<Student**>(luaL_checkudata(L, 1, "StudentPtrMetatable"));
  if (pptr == nullptr) {
    logf("greet nullptr");
    return 0;
  }
  (*pptr)->greet();
  return 0;
}

int lua_student_get_age(lua_State* L) {
  auto pptr =
      static_cast<Student**>(luaL_checkudata(L, 1, "StudentPtrMetatable"));
  if (pptr == nullptr) {
    logf("get age nullptr");
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, (*pptr)->age_);
  return 1;
}

int lua_student_set_age(lua_State* L) {
  auto pptr =
      static_cast<Student**>(luaL_checkudata(L, 1, "StudentPtrMetatable"));
  auto age = lua_tointeger(L, 2);
  if (pptr == nullptr) {
    logf("set age nullptr");
    return 0;
  }
  (*pptr)->age_ = age;
  return 0;
}

int lua_student_get_name(lua_State* L) {
  auto pptr =
      static_cast<Student**>(luaL_checkudata(L, 1, "StudentPtrMetatable"));
  if (pptr == nullptr) {
    logf("get name nullptr");
    lua_pushnil(L);
    return 1;
  }
  lua_pushstring(L, (*pptr)->name_.c_str());
  return 1;
}

int lua_student_set_name(lua_State* L) {
  auto pptr =
      static_cast<Student**>(luaL_checkudata(L, 1, "StudentPtrMetatable"));
  auto name = lua_tostring(L, 2);
  if (pptr == nullptr) {
    logf("set name nullptr");
    return 0;
  }
  (*pptr)->name_ = name;
  return 0;
}

void lua_register_student(lua_State* L) {
  int flag;
  luaL_Reg student_getters[] = {{"age", lua_student_get_age},
                                {"name", lua_student_get_name},
                                {nullptr, nullptr}};
  luaL_Reg student_methods[] = {{"greet", lua_student_greet},
                                {nullptr, nullptr}};
  luaL_Reg student_setters[] = {{"age", lua_student_set_age},
                                {"name", lua_student_set_name},
                                {nullptr, nullptr}};
  flag = luaL_dostring(L,
                       "\
      StudentPtrPrototype = { \n \
        __methods = {}, \n \
        __getters = {}, \n \
        __setters = {}, \n \
      }\n \
      function StudentPtrPrototype.__impl_index(self, key) \n \
        if StudentPtrPrototype.__methods[key] ~= nil then \n \
          return StudentPtrPrototype.__methods[key] \n \
        end \n \
        if StudentPtrPrototype.__getters[key] ~= nil then \n \
          return StudentPtrPrototype.__getters[key](self) \n \
        end \n \
        return nil \n \
      end \n \
      function StudentPtrPrototype.__impl_newindex(self, key, value) \n \
        if StudentPtrPrototype.__setters[key] ~= nil then \n \
          StudentPtrPrototype.__setters[key](self, value) \n \
        end \n \
      end \n \
      ");
  if (flag != 0) {
    logf("dostring error: %s", lua_tostring(L, -1));
    return;
  }
  // Query prototype table and push it to stack.
  lua_getglobal(L, "StudentPtrPrototype");

  // Extract field table `__getters`
  lua_getfield(L, -1, "__getters");
  assert(lua_istable(L, -1));
  // Register getters into `__getters` table.
  luaL_register(L, nullptr, student_getters);
  // Pop table.
  lua_pop(L, 1);

  // Extract field table `__methods`
  lua_getfield(L, -1, "__methods");
  assert(lua_istable(L, -1));
  // Register methods into `__methods` table.
  luaL_register(L, nullptr, student_methods);
  // Pop table.
  lua_pop(L, 1);

  // Extract field table `__setters`
  lua_getfield(L, -1, "__setters");
  assert(lua_istable(L, -1));
  // Register setters into `__setters` table.
  luaL_register(L, nullptr, student_setters);
  // Pop table
  lua_pop(L, 1);

  flag = luaL_newmetatable(L, "StudentPtrMetatable");  // will push
  if (!flag) {
    logf("Meta table has already been created!");
    return;
  }

  // Now stack pos top (-1) is meta-table.
  // Next one (-2) is prototype table.
  // Extract function `__impl_index`
  lua_getfield(L, -2, "__impl_index");  // will push
  assert(lua_isfunction(L, -1));
  lua_getfield(L, -3, "__impl_newindex");  // will push

  // -1: `__impl_newindex`
  // -2: `__impl_index`
  // -3: meta-table
  // -4: prototype table

  lua_setfield(L, -3, "__newindex");  // will pop
  lua_setfield(L, -2, "__index");     // will pop
  // Clear stack
  lua_pop(L, 2);
}

void lua_push_student(lua_State* L, Student* student) {
  auto pptr = static_cast<Student**>(lua_newuserdata(L, sizeof(Student*)));
  assert(pptr != nullptr);
  *pptr = student;
  int flag = luaL_newmetatable(L, "StudentPtrMetatable");
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

  Student student;

  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  lua_register_student(L);
  luaL_dofile(L, file_name);

  lua_getglobal(L, func_name);
  lua_push_student(L, &student);
  int ret = lua_pcall(L, 1, 0, 0);
  if (ret != 0) {
    logf("call error: %s", lua_tostring(L, -1));
  }

  return 0;
}
