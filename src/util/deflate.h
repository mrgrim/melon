//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_UTIL_DEFLATE_H
#define LODE_UTIL_DEFLATE_H

namespace melon::util {
    int gunzip(const std::vector<char> &in, std::vector<uint8_t> *out);
}

#endif //LODE_UTIL_DEFLATE_H
