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
#include "mem/pmr.h"

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

    constexpr std::size_t padding_size = 8;

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
                                      { sizeof(int8_t), tag_category_enum::cat_primitive },
                                      { sizeof(int16_t), tag_category_enum::cat_primitive },
                                      { sizeof(int32_t), tag_category_enum::cat_primitive },
                                      { sizeof(int64_t), tag_category_enum::cat_primitive },
                                      { sizeof(float), tag_category_enum::cat_primitive },
                                      { sizeof(double), tag_category_enum::cat_primitive },
                                      { sizeof(int8_t), tag_category_enum::cat_array },
                                      { sizeof(char), tag_category_enum::cat_string },
                                      { 255, tag_category_enum::cat_list },
                                      { 255, tag_category_enum::cat_compound },
                                      { sizeof(int32_t), tag_category_enum::cat_array },
                                      { sizeof(int64_t), tag_category_enum::cat_array }
                              }};

    template<tag_type_enum tag_idx>
    concept is_nbt_primitive = (tag_idx < tag_properties.size()) && (tag_properties[tag_idx].category == tag_category_enum::cat_primitive);

    template<tag_type_enum tag_idx>
    concept is_nbt_array = (tag_idx < tag_properties.size()) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_array | tag_category_enum::cat_string)) != 0);

    template<tag_type_enum tag_idx>
    concept is_nbt_container = (tag_idx < tag_properties.size()) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_list | tag_category_enum::cat_compound)) != 0);

    template<tag_type_enum tag_idx>
    requires (tag_idx != tag_end) && (tag_idx < tag_properties.size())
    using tag_prim_t = typename std::tuple_element<tag_idx,
            std::tuple<void, int8_t, int16_t, int32_t, int64_t, float, double, int8_t *, char *, list *, compound *, int32_t *, int64_t *>>::type;

    template<tag_type_enum tag_idx>
            requires (tag_idx != tag_end) && (tag_idx < tag_properties.size())
    using tag_iter_t = typename std::tuple_element<tag_idx,
            std::tuple<void, int8_t, int16_t, int32_t, int64_t, float, double, std::span<int8_t>, std::string_view, list *, compound *, std::span<int32_t>, std::span<int64_t>>>::type;

    template<tag_type_enum tag_idx>
    requires (tag_idx != tag_end) && (tag_idx < tag_properties.size())
    using tag_cont_t = typename std::tuple_element<tag_idx, std::tuple<void, primitive_tag, primitive_tag, primitive_tag, primitive_tag, primitive_tag, primitive_tag,
            primitive_tag, primitive_tag, list, compound, primitive_tag, primitive_tag>>::type;

    template<typename T, tag_type_enum tag_idx>
    concept is_nbt_type_match = std::is_same_v<tag_prim_t<tag_idx>, T>;

    // Class does not own the pointers to held array types
    class primitive_tag
    {
    public:
        const tag_type_enum tag_type;
        std::pmr::string *const name;

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

        template<class T, class... Args>
        friend auto mem::pmr::make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args&&... args)
        requires (!std::is_array_v<T>);

        // Must be set to 0 if not a string or array type
        uint32_t size_v;

        explicit primitive_tag(tag_type_enum type_in = tag_byte, uint64_t value_in = 0, std::pmr::string *name_in = nullptr, uint32_t size_in = 0) noexcept
                : tag_type(type_in), name(name_in), size_v(size_in)
        {
            value.generic = value_in;
        }

        // Caller must take care to allocate new memory and assign it to the generic pointer
        void resize(uint32_t new_size)
        { size_v = new_size; }
    };

    template<class T>
    T read_var(char *&itr)
    {
        T var;
        std::memcpy(&var, itr, sizeof(T));
        itr += sizeof(T);
        return util::cvt_endian(var);
    }

    uint64_t inline //__attribute__((always_inline))
    read_tag_primitive(char **itr, tag_type_enum tag_type) noexcept
    {
        uint64_t prim_value;

        // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
        std::memcpy(static_cast<void *>(&prim_value), static_cast<const void *>(*itr), sizeof(prim_value));
        prim_value = util::cvt_endian(prim_value);

        // Pack the value to the left so when read from the union with the proper type it will be correct.
        prim_value = util::pack_left(prim_value, tag_properties[tag_type].size);

        *itr += tag_properties[tag_type].size;

        return prim_value;
    }

    std::tuple<char *, int32_t> inline //__attribute__((always_inline))
    read_tag_array(char **itr, const char *const itr_end, tag_type_enum tag_type, std::pmr::memory_resource *pmr_rsrc)
    {
        auto array_len = read_var<int32_t>(*itr);

        if (array_len < 0) [[unlikely]] throw std::runtime_error("Found array with negative length while parsing binary NBT data.");

        if ((*itr + (array_len * tag_properties[tag_type].size) + padding_size) >= itr_end)
            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

        // My take on a branchless conversion of an unaligned big endian array of an arbitrarily sized data type to an aligned little endian array.
        auto *array_ptr = static_cast<char *>(pmr_rsrc->allocate(array_len * tag_properties[tag_type].size + padding_size, tag_properties[tag_type].size) /* alignment */);
        auto ret        = std::make_tuple(array_ptr, array_len);

        for (auto array_idx = 0; array_idx < array_len; array_idx++)
        {
            uint64_t prim_value = read_tag_primitive(itr, tag_type);
            std::memcpy(static_cast<void *>(array_ptr), static_cast<const void *>(&prim_value), sizeof(prim_value));

            array_ptr += tag_properties[tag_type].size;
        }

        return ret;
    }

    std::tuple<char *, uint16_t> inline //__attribute__((always_inline))
    read_tag_string(char **itr, const char *const itr_end, std::pmr::memory_resource *pmr_rsrc)
    {
        // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
        // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
        auto str_len = read_var<uint16_t>(*itr);

        if ((*itr + str_len + padding_size) >= itr_end)
            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

        auto str_ptr = static_cast<char *>(pmr_rsrc->allocate(str_len + padding_size, alignof(char)));
        std::memcpy(str_ptr, *itr, str_len);
        *itr += str_len;

        return std::make_tuple(str_ptr, str_len);
    }
}

#endif //MELON_NBT_H
