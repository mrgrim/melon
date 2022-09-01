//
// Created by MrGrim on 8/14/2022.
//

#ifndef MELON_NBT_H
#define MELON_NBT_H

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
#include <cstdlib>
#include <tuple>
#include <utility>
#include <variant>
#include <memory>
#include <memory_resource>

#include "util/util.h"

namespace melon::nbt
{
#define NBT_DEBUG true

    class list;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

    enum tag_type_enum : int8_t
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

    enum tag_category_enum : uint8_t
    {
        cat_none      = 0,
        cat_container = 1,
        cat_primitive = 2,
        cat_array     = 3,
        cat_string    = 4
    };

    struct tag_properties_s
    {
        uint8_t           size;
        tag_category_enum category;
    };

    // 127 means recursion is required
    constexpr static std::array<tag_properties_s, 13>
            tag_properties = {{
                                      { 0, tag_category_enum::cat_none },
                                      { 1, tag_category_enum::cat_primitive },
                                      { 2, tag_category_enum::cat_primitive },
                                      { 4, tag_category_enum::cat_primitive },
                                      { 8, tag_category_enum::cat_primitive },
                                      { 4, tag_category_enum::cat_primitive },
                                      { 8, tag_category_enum::cat_primitive },
                                      { 1, tag_category_enum::cat_array },
                                      { 1, tag_category_enum::cat_array },
                                      { 127, tag_category_enum::cat_container },
                                      { 127, tag_category_enum::cat_container },
                                      { 4, tag_category_enum::cat_array },
                                      { 8, tag_category_enum::cat_array }
                              }};

    template<tag_type_enum tag_idx>
    concept is_nbt_primitive = (tag_idx < tag_properties.size()) && (tag_properties[tag_idx].category == tag_category_enum::cat_primitive);

    template<tag_type_enum tag_idx>
    concept is_nbt_array = (tag_idx < tag_properties.size()) && (tag_properties[tag_idx].category == tag_category_enum::cat_array);

    template<tag_type_enum tag_idx>
    concept is_nbt_container = (tag_idx < tag_properties.size()) && (tag_properties[tag_idx].category == tag_category_enum::cat_container);

    template<tag_type_enum tag_idx> requires (!is_nbt_container<tag_idx> && tag_idx != tag_end)
    using tag_prim_t = typename std::tuple_element<tag_idx, std::tuple<void, int8_t, int16_t, int32_t, int64_t, float, double, int8_t, char, void, void, int32_t, int64_t>>::type;

#if NBT_DEBUG == true
    const static char *tag_printable_names[13]{
            "End", "Byte", "Short", "Int", "Long", "Float", "Double", "Byte Array", "String", "List", "Compound", "Int Array", "Long Array"
    };
#endif

#pragma clang diagnostic pop

    struct primitive_tag
    {
        ~primitive_tag();

        tag_type_enum    tag_type = tag_byte;
        uint32_t         size = 1; // Only used for strings and arrays
        std::pmr::string *name = nullptr;

        explicit primitive_tag(tag_type_enum type_in = tag_byte, uint64_t value_in = 0, std::pmr::string *name_in = nullptr, // NOLINT(cppcoreguidelines-pro-type-member-init)
                               [[maybe_unused]] uint32_t size_in = 1)
                : tag_type(type_in), name(name_in), size(size_in)
        {
            value.generic = value_in;
        }

        primitive_tag(const primitive_tag &) = delete;
        primitive_tag &operator=(const primitive_tag &) = delete;

        primitive_tag(primitive_tag &&) noexcept;
        primitive_tag &operator=(primitive_tag &&) noexcept;

        union
        {
            uint64_t generic = 0;

            primitive_tag *compound;
            primitive_tag *list;

            int8_t  tag_byte;
            int16_t tag_short;
            int32_t tag_int;
            int64_t tag_long;

            float  tag_float;
            double tag_double;

            // Warning: This will not be null terminated!
            char *tag_string;

            int8_t  *tag_byte_array;
            int32_t *tag_int_array;
            int64_t *tag_long_array;

            float  *tag_float_array;
            double *tag_double_array;

        } value;
    };

    uint64_t inline __attribute__((always_inline)) read_tag_primitive(std::byte **itr, tag_type_enum tag_type)
    {
        uint64_t prim_value;

        // C++14 guarantees the start address is the same across union members making a memcpy safe to do
        // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
        std::memcpy(reinterpret_cast<void *>(&prim_value), reinterpret_cast<const void *>(*itr), sizeof(prim_value));
        prim_value = cvt_endian(prim_value);

        // Pack the value to the left so when read from the union with the proper type it will be correct.
        prim_value = pack_left(prim_value, tag_properties[tag_type].size);

        *itr += tag_properties[tag_type].size;

        return prim_value;
    }

    std::tuple<std::byte *, int32_t> inline __attribute__((always_inline))
    read_tag_array(std::byte **itr, tag_type_enum tag_type, std::pmr::memory_resource *pmr_rsrc = std::pmr::get_default_resource())
    {
        int32_t array_len;
        std::memcpy(&array_len, *itr, sizeof(array_len));
        array_len = cvt_endian(array_len);
        *itr += sizeof(array_len);

        // My take on a branchless conversion of an unaligned big endian array of an arbitrarily sized data type to an aligned little endian array.
        auto *array_ptr = static_cast<std::byte *>(pmr_rsrc->allocate(array_len * tag_properties[tag_type].size + 8, tag_properties[tag_type].size) /* alignment */);
        auto ret        = std::make_tuple(array_ptr, array_len);

        for (auto array_idx = 0; array_idx < array_len; array_idx++)
        {
            uint64_t prim_value = read_tag_primitive(itr, tag_type);
            std::memcpy(array_ptr, reinterpret_cast<void *>(&prim_value), sizeof(prim_value));

            array_ptr += tag_properties[tag_type].size;
        }

        return ret;
    }

}

#endif //MELON_NBT_H
