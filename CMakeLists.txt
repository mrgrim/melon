cmake_minimum_required(VERSION 3.22)
project(melon)

set(Deflate_OLD_FIND_LIBRARY_PREFIXES "${CMAKE_FIND_LIBRARY_PREFIXES}")
set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")

include_directories(./src;./include)
find_library(DEFLATE_LIBRARY NAMES deflate PATH_SUFFIXES lib)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(melon src/main.cpp src/util/util.h src/util/deflate.cpp src/util/deflate.h src/util/file.cpp src/util/file.h
        src/nbt/compound.h src/nbt/compound.cpp src/nbt/list.h src/nbt/list.cpp src/nbt/nbt.h src/nbt/nbt.cpp src/mem/pmr.h src/mem/pmr.cpp)
target_link_libraries(melon LINK_PUBLIC ${DEFLATE_LIBRARY})
