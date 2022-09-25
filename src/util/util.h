//
// Created by MrGrim on 8/24/2022.
//

#ifndef MELON_UTIL_H
#define MELON_UTIL_H

#include <cstdint>
#include <concepts>
#include <bit>

namespace melon::util
{
    // This very cool method of forcing designated initializers from:
    // https://stackoverflow.com/questions/67521214/c20-force-usage-of-designated-initializers-to-emulate-named-function-argument#comment119348000_67521344
    template<class D>
    class forced_named_init {
        struct Key { explicit Key() = default; };
        struct StopIt { StopIt(Key) { } };
        [[no_unique_address]] StopIt token = Key();
        friend D;
    };

    template<auto src_endian = std::endian::big, typename T>
    requires std::integral<T>
    T cvt_endian(T value)
    {
        if constexpr (std::endian::native != src_endian)
            value = std::byteswap(value);

        return value;
    }

    // This function exists to handle a smaller data type being loaded into a larger data type (e.g. a 16 bit int read into a 64 bit int) via memcpy.
    // If endian conversion required a byte swap, the value will be in the upper (right) bytes of the data type. This will handle moving them into
    // the lower (left) bytes so a second memcpy of the smaller size or a union read will produce the appropriate value.
    template<auto src_endian = std::endian::big, typename T>
    requires std::integral<T>
    T inline pack_left(T value, uint16_t source_size)
    {
        if constexpr (std::endian::native != src_endian)
        {
            if constexpr (src_endian == std::endian::big)
                value >>= ((sizeof(T) - source_size) << 3);
            else
                value <<= ((sizeof(T) - source_size) << 3);
        }

        return value;
    }
}

#endif //MELON_UTIL_H
