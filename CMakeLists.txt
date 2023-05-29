project("LuaIntegration")
cmake_minimum_required(VERSION 3.13)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)


add_library(libluajit STATIC IMPORTED)
set(LUAJIT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/luajit/src")
set_target_properties(libluajit PROPERTIES
   IMPORTED_LOCATION "${LUAJIT_SOURCE_DIR}/libluajit.a"
)
include_directories("${LUAJIT_SOURCE_DIR}")
message(STATUS "LuaJIT source dir: ${LUAJIT_SOURCE_DIR}")

add_executable(manual manual.cpp)
target_link_libraries(manual libluajit ${CMAKE_DL_LIBS}) 
