cmake_minimum_required(VERSION 3.22)
project(melon)

if(MSVC)
    set(MELON_DEBUG_OPTIONS "/Od" "/ZI")
    set(MELON_RELEASE_OPTIONS "/O2" "/Zi")
else()
    set(MELON_DEBUG_OPTIONS "-g" "-O0" "-Wall")
    set(MELON_RELEASE_OPTIONS "-O3" "-DNDEBUG")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")

set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")

add_subdirectory(extern/libdeflate)

include_directories(
        src
        include
        extern/libdeflate/libdeflate
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(melon src/main.cpp src/util/util.h src/util/deflate.cpp src/util/deflate.h src/util/file.cpp src/util/file.h
        src/nbt/compound.h src/nbt/compound.cpp src/nbt/list.h src/nbt/list.cpp src/nbt/nbt.h src/nbt/nbt.cpp src/mem/pmr.h src/mem/pmr.cpp src/util/concepts.h)
target_link_libraries(melon LINK_PUBLIC libdeflate)

target_compile_options(melon BEFORE PRIVATE "$<$<CONFIG:Release>:${MELON_RELEASE_OPTIONS}>")
target_compile_options(melon BEFORE PRIVATE "$<$<CONFIG:Debug>:${MELON_DEBUG_OPTIONS}>")
