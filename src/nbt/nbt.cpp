//
// Created by MrGrim on 8/16/2022.
//

#include "nbt/nbt.h"

namespace melon::nbt
{
    primitive_tag::primitive_tag(primitive_tag&& in) noexcept // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        tag_type = in.tag_type;
        size = in.size;
        value.tag_long = in.value.tag_long;

        in.tag_type = tag_end;
        in.size = 0;
        in.value.tag_long = 0;
    }

    primitive_tag& primitive_tag::operator=(primitive_tag&& in) noexcept
    {
        if (this != &in)
        {
//            if (tag_properties[tag_type].is_complex && (void *)(value.tag_byte_array) != nullptr)
//                free((void *)(value.tag_byte_array));

            tag_type = in.tag_type;
            size = in.size;
            value.tag_long = in.value.tag_long;

            in.tag_type = tag_end;
            in.size = 0;
            in.value.tag_long = 0;
        }

        return *this;
    }

    primitive_tag::~primitive_tag()
    {
#if DEBUG == true
        std::cout << "Deleting primitive." << std::endl;
#endif
//        if (tag_properties[tag_type].is_complex && (void *)(value.tag_string) != nullptr)
//            free((void *)(value.tag_byte_array));
    }

}