cmake_minimum_required(VERSION 3.23)
project(melon)

include_directories(F:/Development/SDK/libdeflate;./src;./include)
find_library(libdeflate deflatestatic F:/Development/SDK/libdeflate)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(melon src/main.cpp src/util/deflate.cpp src/util/deflate.h src/util/file.cpp src/util/file.h src/nbt/compound.h src/nbt/compound.cpp src/nbt/list.h src/nbt/list.cpp src/nbt/nbt.h src/nbt/nbt.cpp)
target_link_libraries(melon LINK_PUBLIC ${libdeflate})
