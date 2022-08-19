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
#include <tuple>
#include <utility>
#include <variant>

namespace melon::nbt
{
    #define NBT_DEBUG false

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
        uint32_t      size;

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

            float  tag_float;
            double tag_double;

            char *tag_string;

            int8_t  *tag_byte_array;
            int32_t *tag_int_array;
            int64_t *tag_long_array;
        }             value;
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

    class nbtcontainer
    {
    public:
        uint64_t    size  = 0;
        std::string *name = nullptr;

        nbtcontainer() = delete;
        [[deprecated("Use melon::nbt:compound instead")]] explicit nbtcontainer(std::vector<uint8_t> *, int64_t max_size_in = -1)
                : depth_v(1), max_size(max_size_in)
        {
            throw std::runtime_error("Invalid initialization of nbtcontainer base class.");
        }

        nbtcontainer(const nbtcontainer &) = delete;
        nbtcontainer &operator=(const nbtcontainer &) = delete;

        nbtcontainer(nbtcontainer &&) noexcept;
        nbtcontainer &operator=(nbtcontainer &&) noexcept;

        [[nodiscard]] uint16_t depth() const
        {
            return depth_v;
        }

        virtual ~nbtcontainer();

    protected:
        explicit nbtcontainer(int64_t max_size_in = -1)
                : depth_v(1), max_size(max_size_in)
        { }

        explicit nbtcontainer(const nbtcontainer *const nbt_in)
                : depth_v(nbt_in->depth_v + 1), max_size(nbt_in->max_size), size_tracking(nbt_in->size_tracking), parent(nbt_in->parent)
        {
            if (depth_v > 512) throw std::runtime_error("NBT Tags Nested Too Deeply (>512).");
        }

        uint16_t           depth_v       = 0;
        uint64_t           size_tracking = 0;
        int64_t            max_size      = -1;
        bool               readonly      = false;
        const nbtcontainer *parent       = nullptr;
    };

    class compound : nbtcontainer
    {
    public:
        explicit compound(int64_t max_size_in = -1)
                : nbtcontainer(max_size_in)
        { }

        explicit compound(std::vector<uint8_t> *raw_in, int64_t max_size_in = -1)
                : nbtcontainer(max_size_in)
        {
            read(raw_in, nullptr, false);
        }

        explicit compound(std::vector<uint8_t> *raw_in, uint8_t **itr_in, const nbtcontainer *const parent_in, bool skip_header = false)
                : nbtcontainer(parent_in)
        {
            *itr_in = read(raw_in, *itr_in, skip_header);
        }

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) noexcept;
        compound &operator=(compound &&) noexcept;

        ~compound() override;
    private:
        uint8_t *read(std::vector<uint8_t> *raw_in, uint8_t *itr = nullptr, bool skip_header = false);

        friend class std::pair<std::string, compound>;

        friend class std::unordered_map<std::string, compound>;

        friend class std::piecewise_construct_t;

        std::unordered_map<std::string, primitive_tag> primitives;
        std::unordered_map<std::string, compound>      compounds;
        std::unordered_map<std::string, list>          lists;
    };

    class list : nbtcontainer
    {
    public:
        tag_type_enum type  = tag_end;
        int32_t       count = 0;

        explicit list(int64_t max_size_in = -1)
                : nbtcontainer(max_size_in)
        { }

        explicit list(std::vector<uint8_t> *raw_in, int64_t max_size_in = -1)
                : nbtcontainer(max_size_in)
        {
            read(raw_in, nullptr, false);
        }

        explicit list(std::vector<uint8_t> *raw_in, uint8_t **itr_in, const nbtcontainer *const parent_in, bool skip_header = false)
                : nbtcontainer(parent_in)
        {
            *itr_in = read(raw_in, *itr_in, skip_header);
        }

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) noexcept;
        list &operator=(list &&) noexcept;

        ~list() override;

    private:
        uint8_t *read(std::vector<uint8_t> *raw, uint8_t *itr, bool skip_header = false);

        std::vector<primitive_tag> primitives;
        std::vector<list>          lists;
        std::vector<compound>      compounds;
    };
}

#endif //LODE_NBT_LIST_H
