//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_NBT_LIST_H
#define LODE_NBT_LIST_H

#include <vector>
#include <array>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <string>
#include <concepts>
#include <bit>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <stdlib.h>

namespace melon::nbt
{

    class list;

    struct tag_properties_s
    {
        int8_t size;
        bool   is_complex;
    };

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

    enum tag_type_enum : uint8_t
    {
        tag_end        = 0,
        tag_byte       = 1,
        tag_short      = 2,
        tag_int        = 3,
        tag_long       = 4,
        tag_float      = 5,
        tag_double     = 6,
        tag_byte_array = 7,
        tag_string     = 8,
        tag_list       = 9,
        tag_compound   = 10,
        tag_int_array  = 11,
        tag_long_array = 12
    };

#pragma clang diagnostic pop

    struct primitive_tag
    {
        ~primitive_tag();

        tag_type_enum tag_type;
        uint32_t size;

        explicit primitive_tag(tag_type_enum type_in, uint64_t value_in, uint32_t size_in = 1) // NOLINT(cppcoreguidelines-pro-type-member-init)
        {
            tag_type = type_in;
            memcpy((void *)&value, (void *)&value_in, sizeof(value_in));
            size = size_in;
        }

        primitive_tag(const primitive_tag &) = delete;
        primitive_tag &operator=(const primitive_tag &) = delete;

        primitive_tag(primitive_tag &&) noexcept;
        primitive_tag &operator=(primitive_tag &&) noexcept;

        union
        {
            int8_t  tag_byte;
            int16_t tag_short;
            int32_t tag_int;
            int64_t tag_long;

            float   tag_float;
            double  tag_double;

            char    *tag_string;

            int8_t  *tag_byte_array;
            int32_t *tag_int_array;
            int64_t *tag_long_array;
        } value;
    };

    // Negative numbers represent element size for vector types
    // 127 means recursion is required
    const static std::array<tag_properties_s, 13> tag_properties = {{
                                                                            { 0, false },
                                                                            { 1, false },
                                                                            { 2, false },
                                                                            { 4, false },
                                                                            { 8, false },
                                                                            { 4, false },
                                                                            { 8, false },
                                                                            { -1, true },
                                                                            { -1, true },
                                                                            { 127, true },
                                                                            { 127, true },
                                                                            { -4, true },
                                                                            { -8, true }
                                                                    }};

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

#pragma clang diagnostic pop

    template<typename T>
    using uint_size =
            typename std::conditional<(sizeof(T) == 2), uint16_t,
                    typename std::conditional<(sizeof(T) == 4), uint32_t,
                            uint64_t
                    >::type
            >::type;

    template<typename T>
    void read_tag_array(int32_t len, void **dst, void *src)
    {
        *dst = (void *)malloc(len * sizeof(T));
        std::memcpy((void *)*dst, src, len * sizeof(T));

        if constexpr (sizeof(T) >= 2)
            for (int index = 0; index < len; index++)
                ((T *)(*dst))[index] = cvt_endian<T>(((T *)(*dst))[index]);
    }

    class compound
    {
    public:
        uint64_t    size  = 0;
        std::string *name = nullptr;

        std::unordered_map<std::string, primitive_tag> primitives;
        std::unordered_map<std::string, compound>      compounds;
        std::unordered_map<std::string, list>          lists;

        explicit compound(int depth_in = 0)
                : depth(depth_in)
        { }

        explicit compound(int depth_in, std::vector<uint8_t> *raw_in, uint8_t **itr_in, bool skip_header = false)
        {
            *itr_in = read(raw_in, *itr_in, depth_in, skip_header);
        }

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) noexcept;
        compound &operator=(compound &&) noexcept;

        virtual ~compound();

        uint8_t *read(std::vector<uint8_t> *raw_in, uint8_t *itr_in = nullptr, uint32_t depth_in = 0, bool skip_header = false);

    private:
        uint16_t depth         = 0;
        uint64_t size_tracking = 0;
        bool     readonly      = false;
    };

    class list
    {
    public:
        tag_type_enum type  = tag_end;
        int32_t       count = 0;
        uint64_t      size  = 0;
        std::string   *name = nullptr;

        explicit list(int depth_in = 0)
                : depth(depth_in)
        { }

        explicit list(int depth_in, std::vector<uint8_t> *raw_in, uint8_t **itr_in, bool skip_header = false)
        {
            *itr_in = read(raw_in, *itr_in, depth_in, skip_header);
        }

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) noexcept;
        list &operator=(list &&) noexcept;

        virtual ~list();

        uint8_t *read(std::vector<uint8_t> *raw, uint8_t *itr, uint32_t depth_in = 0, bool skip_header = false);

    private:
        uint16_t depth         = 0;
        uint64_t size_tracking = 0;
        bool     readonly      = false;

        std::vector<primitive_tag> primitives;
        std::vector<list>          lists;
        std::vector<compound>      compounds;
    };
}

#endif //LODE_NBT_LIST_H
