//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_PRIMITIVE_H
#define MELON_NBT_PRIMITIVE_H

#include <string>
#include "types.h"
#include "concepts.h"
#include "util/util.h"
#include "mem/pmr.h"

namespace melon::nbt
{
    // Class does not own the pointers to held array types. This is to avoid storing the state necessary to do so with PMR.
    // Exploit the fact that no sane compiler will mess this up, despite this being the standards most idiotic instance of UB
    class primitive
    {
        struct size_params : util::forced_named_init<size_params> {
            bool full_tag = true;
        };

    public:
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

        primitive() = delete;
        ~primitive() = default;

        primitive(const primitive &) = delete;
        primitive &operator=(const primitive &) = delete;

        primitive(primitive &&) = delete;
        primitive &operator=(primitive &&) = delete;

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
                return std::string_view{ value.tag_string, static_cast<size_t>(size_v) };
            else if constexpr (tag_type == tag_byte_array)
                return std::span(value.tag_byte_array, static_cast<size_t>(size_v));
            else if constexpr (tag_type == tag_int_array)
                return std::span(value.tag_int_array, static_cast<size_t>(size_v));
            else if constexpr (tag_type == tag_long_array)
                return std::span(value.tag_long_array, static_cast<size_t>(size_v));
        }

        tag_variant_t get_generic();

        [[nodiscard]] size_t bytes(size_params params = { .full_tag = true }) const
        {
            size_t name_size = params.full_tag ? sizeof(int8_t) + sizeof(uint16_t) + name->size() : 0;
            if (size() == 0)
                return name_size + tag_properties[type()].size;
            else if (type() == tag_string)
                return name_size + sizeof(uint16_t) + size();
            else
                return name_size + sizeof(int32_t) + (size() * tag_properties[type()].size);
        }

        [[nodiscard]] tag_type_enum type() const
        { return type_v; }

        [[nodiscard]] int32_t size() const
        { return size_v; }

    private:
        friend class list;

        friend class compound;

        template<class T, class... Args>
        friend auto mem::pmr::make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
        requires (!std::is_array_v<T>);

        const tag_type_enum type_v;

        // Must be set to 0 if not a string or array type
        int32_t size_v;

        explicit primitive(tag_type_enum type_in = tag_byte, uint64_t value_in = 0, std::pmr::string *name_in = nullptr, size_t size_in = 0) noexcept
                : name(name_in), type_v(type_in), size_v(size_in)
        { value.generic = value_in; }

        // Caller must take care to allocate new memory and assign it to the generic pointer
        void set_size(int32_t new_size)
        { size_v = new_size; }

        void to_snbt(std::string &out) const;
        char *to_binary(char *itr) const;
    };
}

#endif //MELON_NBT_PRIMITIVE_H
