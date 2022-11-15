project("PlayGround")
cmake_minimum_required(VERSION 3.20)

# if(NOT CMAKE_BUILD_TYPE)
# 	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
# 		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
# endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)

add_executable(main main.cpp)
