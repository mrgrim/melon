//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_UTIL_DEFLATE_H
#define LODE_UTIL_DEFLATE_H

#include "libdeflate.h"

namespace melon::util {
    void gzip_inflate(std::vector<std::byte> &out, const std::vector<char> &in, libdeflate_decompressor *d = nullptr);
    void gzip_deflate(std::vector<char *> &out, std::vector<std::byte> &in, libdeflate_compressor *c = nullptr, int level = 6);
}

#endif //LODE_UTIL_DEFLATE_H
