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
#include <span>

#include "util/util.h"

namespace melon::nbt
{
    class list;

    class compound;

    class primitive_tag;

    namespace snbt
    {
        void escape_string(const std::string_view &in_str, std::string &out_str, bool always_quote = true);

        namespace syntax
        {
            constexpr std::string_view string_unquoted_chars  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxzy+-_.";
            constexpr std::string_view string_chars_to_escape = R"("'\)";
            constexpr char             string_escape_char     = '\\';
            constexpr char             string_std_quote       = '"';
            constexpr char             string_alt_quote       = '\'';
        }
    }

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
        cat_none      = 0b000001,
        cat_compound  = 0b000010,
        cat_list      = 0b000100,
        cat_primitive = 0b001000,
        cat_array     = 0b010000,
        cat_string    = 0b100000
    };

    struct tag_properties_s
    {
        uint8_t           size;
        tag_category_enum category;
    };

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
                                      { 1, tag_category_enum::cat_string },
                                      { 255, tag_category_enum::cat_list },
                                      { 255, tag_category_enum::cat_compound },
                                      { 4, tag_category_enum::cat_array },
                                      { 8, tag_category_enum::cat_array }
                              }};

    template<tag_type_enum tag_idx>
    concept is_nbt_primitive = (tag_idx < tag_properties.size()) && (tag_properties[tag_idx].category == tag_category_enum::cat_primitive);

    template<tag_type_enum tag_idx>
    concept is_nbt_array = (tag_idx < tag_properties.size()) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_array | tag_category_enum::cat_string)) != 0);

    template<tag_type_enum tag_idx>
    concept is_nbt_container = (tag_idx < tag_properties.size()) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_list | tag_category_enum::cat_compound)) != 0);

    template<tag_type_enum tag_idx>
            requires (!is_nbt_container<tag_idx> && tag_idx != tag_end)
    using tag_prim_t = typename std::tuple_element<tag_idx,
            std::tuple<void, int8_t, int16_t, int32_t, int64_t, float, double, int8_t *, char *, void, void, int32_t *, int64_t *>>::type;

    template<tag_type_enum tag_idx>
            requires (tag_idx != tag_end)
    using tag_cont_t = typename std::tuple_element<tag_idx, std::tuple<void, primitive_tag, primitive_tag, primitive_tag, primitive_tag, primitive_tag, primitive_tag,
            primitive_tag, primitive_tag, list, compound, primitive_tag, primitive_tag>>::type;

    template<typename T, tag_type_enum tag_idx>
    concept is_nbt_type_match = std::is_same_v<tag_prim_t<tag_idx>, T>;

    class primitive_tag
    {
    public:
        const tag_type_enum tag_type;
        const std::pmr::string *const name;

        union
        {
            uint64_t generic = 0;
            void     *generic_ptr;

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

        } value;

        primitive_tag() = delete;
        ~primitive_tag() = default;

        primitive_tag(const primitive_tag &) = delete;
        primitive_tag &operator=(const primitive_tag &) = delete;

        primitive_tag(primitive_tag &&) = delete;
        primitive_tag &operator=(primitive_tag &&) = delete;

        void to_snbt(std::string &out);

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        auto &get()
        {
            if constexpr (tag_type == tag_byte)
                return value.tag_byte;
            else if constexpr (tag_type == tag_short)
                return value.tag_short;
            else if constexpr (tag_type == tag_int)
                return value.tag_int;
            else if constexpr (tag_type == tag_long)
                return value.tag_long;
            else if constexpr (tag_type == tag_float)
                return value.tag_float;
            else if constexpr (tag_type == tag_double)
                return value.tag_double;
        };

        template<tag_type_enum tag_type>
        requires is_nbt_array<tag_type>
        auto get()
        {
            if constexpr (tag_type == tag_string)
                return std::string_view{ value.tag_string, size_v };
            else if constexpr (tag_type == tag_byte_array)
                return std::span(value.tag_byte_array, size_v);
            else if constexpr (tag_type == tag_int_array)
                return std::span(value.tag_int_array, size_v);
            else if constexpr (tag_type == tag_long_array)
                return std::span(value.tag_long_array, size_v);
        }

        [[nodiscard]] auto size() const
        { return size_v; }

    private:
        friend class list;

        friend class compound;

        // Must be set to 0 if not a string or array type
        uint32_t size_v;

        explicit primitive_tag(tag_type_enum type_in = tag_byte, uint64_t value_in = 0, std::pmr::string *name_in = nullptr, uint32_t size_in = 0)
                : tag_type(type_in), name(name_in), size_v(size_in)
        {
            value.generic = value_in;
        }

        // Caller must take care to allocate new memory and assign it to the generic pointer
        void resize(uint32_t new_size)
        { size_v = new_size; }
    };

    uint64_t inline __attribute__((always_inline)) read_tag_primitive(char **itr, tag_type_enum tag_type)
    {
        uint64_t prim_value;

        // C++14 guarantees the start address is the same across union members making a memcpy safe to do
        // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
        std::memcpy(static_cast<void *>(&prim_value), static_cast<const void *>(*itr), sizeof(prim_value));
        prim_value = cvt_endian(prim_value);

        // Pack the value to the left so when read from the union with the proper type it will be correct.
        prim_value = pack_left(prim_value, tag_properties[tag_type].size);

        *itr += tag_properties[tag_type].size;

        return prim_value;
    }

    std::tuple<char *, int32_t> inline __attribute__((always_inline))
    read_tag_array(char **itr, const char *const itr_end, tag_type_enum tag_type, std::pmr::memory_resource *pmr_rsrc = std::pmr::get_default_resource())
    {
        int32_t array_len;
        std::memcpy(static_cast<void *>(&array_len), static_cast<const void *>(*itr), sizeof(array_len));
        array_len = cvt_endian(array_len);

        if ((*itr + sizeof(array_len) + (array_len * tag_properties[tag_type].size) + 8) >= itr_end)
            throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

        *itr += sizeof(array_len);

        // My take on a branchless conversion of an unaligned big endian array of an arbitrarily sized data type to an aligned little endian array.
        auto *array_ptr = static_cast<char *>(pmr_rsrc->allocate(array_len * tag_properties[tag_type].size + 8, tag_properties[tag_type].size) /* alignment */);
        auto ret        = std::make_tuple(array_ptr, array_len);

        for (auto array_idx = 0; array_idx < array_len; array_idx++)
        {
            uint64_t prim_value = read_tag_primitive(itr, tag_type);
            std::memcpy(static_cast<void *>(array_ptr), static_cast<const void *>(&prim_value), sizeof(prim_value));

            array_ptr += tag_properties[tag_type].size;
        }

        return ret;
    }

}

#endif //MELON_NBT_H
