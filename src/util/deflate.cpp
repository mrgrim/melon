//
// Created by MrGrim on 8/14/2022.
//

#include <vector>
#include <memory>
#include <iostream>
#include <cstring>
#include <span>
#include <utility>

#include "deflate.h"
#include "util.h"

namespace melon::util
{

    std::pair<std::unique_ptr<char[]>, size_t> gzip_inflate(std::unique_ptr<char[]> &&buf_ptr, size_t buf_size, libdeflate_decompressor *d)
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
        std::memcpy(static_cast<void *>(&isize), static_cast<const void *>(&buf_ptr[buf_size - 4]), sizeof(isize));
        isize = cvt_endian<std::endian::little>(isize);

        if (isize == 0) isize               = 1;
        if (isize > buf_size * 1023) isize = buf_size * 1023; // This is the largest DEFLATE can expand to

        auto out_buf = std::make_unique<char[]>(isize + 8);
        size_t actual_size;

        switch (libdeflate_gzip_decompress(d, static_cast<const void *>(buf_ptr.get()), buf_size,
                                           static_cast<void *>(out_buf.get()), isize, &actual_size))
        {
            case LIBDEFLATE_SHORT_OUTPUT:
            case LIBDEFLATE_SUCCESS:
                libdeflate_free_decompressor(d);
                return {std::move(out_buf), isize + 8};

            case LIBDEFLATE_BAD_DATA:
                libdeflate_free_decompressor(d);
                throw std::runtime_error("Corrupted data found while trying to decompress gzip buffer.");

            case LIBDEFLATE_INSUFFICIENT_SPACE:
                libdeflate_free_decompressor(d);
                throw std::runtime_error("Insufficient buffer space for decompression of gzip buffer.");

            default:
                std::unreachable();
        }
    }

    // This over allocates, probably by a lot. It's expected that the contents will quickly be copied elsewhere and the buffer discarded by the caller.
    std::pair<std::unique_ptr<char[]>, size_t> gzip_deflate(std::unique_ptr<char[]> &&buf_ptr, size_t buf_size, libdeflate_compressor *c, int level)
    {
        if (c == nullptr)
            c = libdeflate_alloc_compressor(level);

        if (c == nullptr)
            [[unlikely]]
                    throw std::runtime_error("Failure to create compressor object while trying to compress to gzip buffer.");

        auto est_size = libdeflate_gzip_compress_bound(c, buf_size);
        auto out = std::make_unique<char[]>(est_size);
        auto out_size = libdeflate_gzip_compress(c, static_cast<const void *>(buf_ptr.get()), buf_size, static_cast<void *>(out.get()), est_size);

        if (out_size == 0)
            [[unlikely]]
                    throw std::runtime_error("Compression output size exceeded upper bound while trying to compress to gzip buffer.");

        return { std::move(out), out_size };
    }
}