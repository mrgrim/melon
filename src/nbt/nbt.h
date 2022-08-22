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

            // Warning: This will not be null terminated!
            char *tag_string;

            int8_t  *tag_byte_array;
            int32_t *tag_int_array;
            int64_t *tag_long_array;
        }             value;
    };

    // Negative numbers represent element size for vector types
    // 127 means recursion is required
    const static std::array<tag_properties_s, 13>
            tag_properties = {{
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
    using uint_size [[maybe_unused]] =
            typename std::conditional<(sizeof(T) == 2), uint16_t,
                    typename std::conditional<(sizeof(T) == 4), uint32_t,
                            uint64_t
                    >::type
            >::type;

    template<typename T>
    void cvt_endian_array(int32_t len, T *src)
    {
        if constexpr (sizeof(T) >= 2)
            for (int index = 0; index < len; index++)
                src[index] = cvt_endian<T>(src[index]);
    }

    template<typename T>
    void read_tag_array(int32_t len, void **dst, void *src)
    {
        *dst = (void *)malloc(len * sizeof(T));
        std::memcpy((void *)*dst, src, len * sizeof(T));

        if constexpr (sizeof(T) >= 2)
            for (int index = 0; index < len; index++)
                ((T *)(*dst))[index] = cvt_endian<T>(((T *)(*dst))[index]);
    }

    std::variant<std::pmr::memory_resource *, std::shared_ptr<std::pmr::memory_resource>> get_std_default_pmr_rsrc();

    class debug_monotonic_buffer_resource : public std::pmr::monotonic_buffer_resource
    {
    public:
        debug_monotonic_buffer_resource(void *buffer, std::size_t buffer_size);
        ~debug_monotonic_buffer_resource() override;

    protected:
        void *do_allocate(std::size_t bytes, std::size_t alignment) override;
        void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;
        bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override;

    private:
        int64_t total_bytes_allocated = 0;
    };
}

#endif //MELON_NBT_H
