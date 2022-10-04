//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_UTIL_DEFLATE_H
#define LODE_UTIL_DEFLATE_H

#include <span>
#include "libdeflate.h"

namespace melon::util {
    std::pair<std::unique_ptr<char[]>, size_t> gzip_inflate(std::unique_ptr<char[]> &&buf_ptr, size_t buf_size, libdeflate_decompressor *d = nullptr);
    std::pair<std::unique_ptr<char[]>, size_t> gzip_deflate(std::unique_ptr<char[]> &&buf_ptr, size_t buf_size, libdeflate_compressor *c = nullptr, int level = 6);
}

#endif //LODE_UTIL_DEFLATE_H
