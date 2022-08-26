//
// Created by MrGrim on 8/14/2022.
//

#include <vector>
#include <bit>
#include <iostream>
#include <cstring>

#include "deflate.h"
#include "util.h"

namespace melon::util
{

#pragma clang diagnostic push
#pragma ide diagnostic ignored "Simplify"

    void gzip_inflate(std::vector<std::byte> &out, const std::vector<char> &in, libdeflate_decompressor *d)
    {
        if (d == nullptr)
            d = libdeflate_alloc_decompressor();

        if (d == nullptr)
            [[unlikely]]
                    throw std::runtime_error("Failure to create decompressor object while trying to decompress gzip buffer.");

        // Extract ISIZE field from the end of the gzip stream
        // This can be misleading if the stream has multiple "members" or if the field overflows (>4GiB)
        // We're not handling either case for now
        uint32_t isize;
        std::memcpy(reinterpret_cast<void *>(&isize), reinterpret_cast<const void *>(&in[in.size() - 4]), sizeof(isize));

        if (isize == 0) isize               = 1;
        if (isize > in.size() * 1023) isize = in.size() * 1023; // This is the largest DEFLATE can expand to

        out.resize(isize + 8); // lil' extra buffer for post-processing
        size_t actual_size;

        switch (libdeflate_gzip_decompress(d, reinterpret_cast<const void *>(in.data()), in.size(),
                                           reinterpret_cast<void *>(out.data()), out.capacity(), &actual_size))
        {
            case LIBDEFLATE_SHORT_OUTPUT:
            case LIBDEFLATE_SUCCESS:
                libdeflate_free_decompressor(d);
                return;

            case LIBDEFLATE_BAD_DATA:
                libdeflate_free_decompressor(d);
                throw std::runtime_error("Corrupted data found while trying to decompress gzip buffer.");

            case LIBDEFLATE_INSUFFICIENT_SPACE:
                libdeflate_free_decompressor(d);
                throw std::runtime_error("Insufficient buffer space for decompression of gzip buffer.");
        }
    }

#pragma clang diagnostic pop

    // out.capacity() after this call is likely to be significantly larger than out.size(). It's up to the caller
    // if they wish to perform a out.shrink_to_fit() call after.
    void gzip_deflate(std::vector<char> &out, std::vector<std::byte> &in, libdeflate_compressor *c = nullptr, int level = 6)
    {
        if (c == nullptr)
            c = libdeflate_alloc_compressor(level);

        if (c == nullptr)
            [[unlikely]]
                    throw std::runtime_error("Failure to create compressor object while trying to compress to gzip buffer.");

        out.reserve(libdeflate_gzip_compress_bound(c, in.size()));
        out.push_back(0); // std::vector has been seen in the wild being lazy about allocating memory, force the issue

        auto out_size = libdeflate_gzip_compress(c, reinterpret_cast<const void *>(in.data()), in.size(), reinterpret_cast<void *>(out.data()), out.size());

        if (out_size == 0)
            [[unlikely]]
                    throw std::runtime_error("Compression output size exceeded upper bound while trying to compress to gzip buffer.");

        out.resize(out_size);
    }
}