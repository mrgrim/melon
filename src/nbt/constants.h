//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_CONSTANTS_H
#define MELON_NBT_CONSTANTS_H

#include <cstdint>
#include <array>

namespace melon::nbt
{
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
    constexpr static size_t tag_count    = 13;

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

    constexpr static std::array<tag_properties_s, tag_count>
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

}

#endif //MELON_NBT_CONSTANTS_H
