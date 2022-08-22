//
// Created by MrGrim on 8/14/2022.
//

#include <vector>
#include <bit>
#include <libdeflate.h>
#include <iostream>

#include "deflate.h"

namespace melon::util {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "Simplify"

    int gunzip(const std::vector<char> &in, std::vector<std::byte> *out) {
        struct libdeflate_decompressor *d;

        d = libdeflate_alloc_decompressor();
        if (d == nullptr)
            return 1;

        // Extract ISIZE field from the end of the gzip stream
        // This can be misleading if the stream has multiple "members" or if the field overflows (>4GiB)
        // We're not handling either case for now
        uint64_t isize = *(reinterpret_cast<const uint32_t *>(&in[in.size() - 4]));

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
        if constexpr (std::endian::native == std::endian::big) isize = std::byteswap(isize);
#pragma clang diagnostic pop

        if (isize == 0) isize = 1;
        if (isize > in.size() * 1023) isize = in.size() * 1023; // This is the largest DEFLATE can expand to

        out->resize(isize);
        size_t actual_size;

        switch (libdeflate_gzip_decompress(d, reinterpret_cast<const void *>(in.data()), in.size(),
                                           reinterpret_cast<void *>(out->data()), out->size(), &actual_size)) {
            case LIBDEFLATE_SHORT_OUTPUT:
            case LIBDEFLATE_SUCCESS:
                if (actual_size != out->size())
                    out->resize(actual_size);

                libdeflate_free_decompressor(d);

                return 0;

            case LIBDEFLATE_BAD_DATA:
                std::cerr << "Corrupted Data" << std::endl;
                break;

            case LIBDEFLATE_INSUFFICIENT_SPACE:
                std::cerr << "Insufficient Buffer Space for Decompression" << std::endl;
                break;
        }

        return -1;
    }
#pragma clang diagnostic pop
}