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

    // Taken from https://artificial-mind.net/blog/2020/10/31/constexpr-for
    template <auto Start, auto End, auto Inc, class F>
    constexpr void constexpr_for(F&& f)
    {
        if constexpr (Start < End)
        {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }

    template <class F, class... Args>
    constexpr void constexpr_for(F&& f, Args&&... args)
    {
        (f(std::forward<Args>(args)), ...);
    }

    template <class F, class Tuple>
    constexpr void constexpr_for_tuple(F&& f, Tuple&& tuple)
    {
        constexpr size_t cnt = std::tuple_size_v<std::decay_t<Tuple>>;

        constexpr_for<size_t(0), cnt, size_t(1)>([&](auto i) {
            f(std::get<i.value>(tuple));
        });
    }

    template<auto src_endian = std::endian::big, auto target_endian = std::endian::native, typename T>
    requires std::integral<T>
    T cvt_endian(T value)
    {
        if constexpr (target_endian != src_endian)
            value = std::byteswap(value);

        return value;
    }

    // This function exists to handle a smaller data type being loaded into a larger data type (e.g. a 16 bit int read into a 64 bit int) via memcpy.
    // If endian conversion required a byte swap, the value will be in the upper (right) bytes of the data type. This will handle moving them into
    // the lower (left) bytes so a second memcpy of the smaller size or a union read will produce the appropriate value.
    template<auto src_endian = std::endian::big, auto target_endian = std::endian::native, typename T>
    requires std::integral<T>
    T inline pack_left(T value, uint16_t source_size)
    {
        if constexpr (target_endian != src_endian)
        {
            // We must have done a byteswap to get here

            if constexpr (std::endian::native == std::endian::little)
            {
                // Byte swap put our value in the higher order bytes. Shift them right to lower order bytes to be lower in RAM.
                value >>= ((sizeof(T) - source_size) << 3);
            }
            else
            {
                // Byte swap put our value in the lower order bytes. Shift them left to higher order bytes to be lower in RAM.
                value <<= ((sizeof(T) - source_size) << 3);
            }
        }

        return value;
    }

    template<template<class> class, template<class...> class, class>
    struct transform_tuple_types { };

    template<template<class> class Transformer, template<class...> class Container, class... Types>
    struct transform_tuple_types<Transformer, Container, std::tuple<Types...>>
    {
        using type = Container<Transformer<Types>...>;
    };

    template<std::size_t Num, class... Types, std::size_t... Offset>
    requires (Num < sizeof...(Types))
    std::tuple<std::tuple_element_t<Num + Offset, std::tuple<Types...>>...> pack_typelist_pop_front_recurse(std::index_sequence<Offset...>) {}

    template<std::size_t Num, class... Types>
    using pack_typelist_pop_front = decltype(pack_typelist_pop_front_recurse<Num, Types...>(std::make_index_sequence<sizeof...(Types) - Num>{ }));

    template<std::size_t Num, class Tuple, std::size_t... Offset>
    requires (Num < std::tuple_size_v<Tuple>)
    std::tuple<std::tuple_element_t<Num + Offset, Tuple>...> tuple_typelist_pop_front_recurse(std::index_sequence<Offset...>) {}

    template<std::size_t Num, class Tuple>
    using tuple_typelist_pop_front = decltype(tuple_typelist_pop_front_recurse<Num, Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple> - Num>{ }));

    // Taken from example at: https://en.cppreference.com/w/cpp/utility/variant/visit
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}

#endif //MELON_UTIL_H
