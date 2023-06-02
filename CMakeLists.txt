cmake_minimum_required(VERSION 3.13)

project("LuaIntegration")
include(ExternalProject)

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/boost_1_82_0")
set(BOOST_LIBRARY_DIRS "${CMAKE_SOURCE_DIR}/boost_1_82_0/lib")
find_package(Boost REQUIRED) 
message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
include_directories("${Boost_INCLUDE_DIRS}")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

add_library(libluajit STATIC IMPORTED)
set(LUAJIT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/luajit/src")
set_target_properties(libluajit PROPERTIES
   IMPORTED_LOCATION "${LUAJIT_SOURCE_DIR}/libluajit.a"
)
include_directories("${LUAJIT_SOURCE_DIR}")
message(STATUS "LuaJIT source dir: ${LUAJIT_SOURCE_DIR}")

add_executable(method src/manual/method.cpp)
target_link_libraries(method libluajit ${CMAKE_DL_LIBS}) 

add_executable(lua_err src/manual/lua_err.cpp)
target_link_libraries(lua_err libluajit ${CMAKE_DL_LIBS})

add_executable(getter_setter src/manual/getter_setter.cpp)
target_link_libraries(getter_setter libluajit ${CMAKE_DL_LIBS})

add_executable(virtual_func src/manual/virtual_func.cpp)
target_link_libraries(virtual_func libluajit ${CMAKE_DL_LIBS})

add_executable(reflection src/reflection/reflection.cpp)

add_executable(oop_lua src/reflection/oop_lua.cpp)
target_link_libraries(oop_lua libluajit ${CMAKE_DL_LIBS})

add_executable(perf src/perf/perf.cpp)
target_link_libraries(perf libluajit ${CMAKE_DL_LIBS})
