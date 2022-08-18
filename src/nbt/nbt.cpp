//
// Created by MrGrim on 8/16/2022.
//

#include <stdlib.h>

#include "nbt/nbt.h"

namespace melon::nbt
{
    primitive_tag::primitive_tag(primitive_tag&& in) noexcept
    {
        tag_type = in.tag_type;
        value.tag_long = in.value.tag_long;

        in.tag_type = tag_end;
        in.value.tag_long = 0;
    }

    primitive_tag& primitive_tag::operator=(primitive_tag&& in) noexcept
    {
        if (this != &in)
        {
            if (tag_properties[tag_type].is_complex && (void *)(value.tag_byte_array) != nullptr)
                free((void *)(value.tag_byte_array));

            tag_type = in.tag_type;
            value.tag_long = in.value.tag_long;

            in.tag_type = tag_end;
            in.value.tag_long = 0;
        }

        return *this;
    }

    primitive_tag::~primitive_tag()
    {
        //std::cout << "Deleting primitive." << std::endl;
        if (tag_properties[tag_type].is_complex && (void *)(value.tag_string) != nullptr)
            free((void *)(value.tag_byte_array));
    }
}