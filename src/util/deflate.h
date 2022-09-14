//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_UTIL_DEFLATE_H
#define LODE_UTIL_DEFLATE_H

#include <span>
#include "libdeflate.h"

namespace melon::util {
    std::pair<std::unique_ptr<char[]>, size_t> gzip_inflate(const std::vector<char> &in, libdeflate_decompressor *d = nullptr);
    void gzip_deflate(std::vector<char> &out, const std::vector<char> &in, libdeflate_compressor *c = nullptr, int level = 6);
}

#endif //LODE_UTIL_DEFLATE_H
