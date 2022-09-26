cmake_minimum_required(VERSION 3.22)
project(libdeflate)

if(MSVC)
    set(LIBDEFLATE_DEBUG_OPTIONS "/Od" "/ZI")
    set(LIBDEFLATE_RELEASE_OPTIONS "/O2" "/Zi")
else()
    set(LIBDEFLATE_DEBUG_OPTIONS "-g" "-Og")
    set(LIBDEFLATE_RELEASE_OPTIONS "-O2" "-DNDEBUG")
endif()

include_directories(libdeflate)

set(LIBDEFLATE_SOURCES
        libdeflate/lib/deflate_decompress.c
        libdeflate/lib/utils.c
        libdeflate/lib/x86/cpu_features.c
        libdeflate/lib/zlib_compress.c
        libdeflate/lib/arm/cpu_features.c
        libdeflate/lib/deflate_compress.c
        libdeflate/lib/adler32.c
        libdeflate/lib/zlib_decompress.c
        libdeflate/lib/zlib_compress.c
        libdeflate/lib/crc32.c
        libdeflate/lib/gzip_decompress.c
        libdeflate/lib/gzip_compress.c
        )

add_library(libdeflate)
target_sources(libdeflate PRIVATE ${LIBDEFLATE_SOURCES})

target_compile_options(libdeflate BEFORE PRIVATE "$<$<CONFIG:Release>:${LIBDEFLATE_RELEASE_OPTIONS}>")
target_compile_options(libdeflate BEFORE PRIVATE "$<$<CONFIG:Debug>:${LIBDEFLATE_DEBUG_OPTIONS}>")
