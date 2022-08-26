//
// Created by MrGrim on 8/24/2022.
//

#ifndef MELON_UTIL_H
#define MELON_UTIL_H

#include <cstdint>
#include <concepts>
#include <bit>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "Simplify"
#pragma ide diagnostic ignored "UnreachableCode"

template<typename T>
requires std::integral<T>
T cvt_endian(T value)
{
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);
    else
        return value;
}

template<typename T>
requires std::integral<T>
T inline pack_left(T value, uint16_t source_size)
{
    if constexpr (std::endian::native == std::endian::little)
        return value >> ((sizeof(T) - source_size) << 3);
    else
        return value;
}

#pragma clang diagnostic pop

#endif //MELON_UTIL_H
