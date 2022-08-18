//
// Created by MrGrim on 8/16/2022.
//

#include <algorithm>
#include <vector>

#include "nbt/nbt.h"

namespace melon::nbt
{
    primitive_tag::~primitive_tag()
    {
        switch (this->tag_type)
        {
            case tag_string:
                delete value.tag_string;
                break;
            case tag_byte_array:
                delete value.tag_byte_array;
                break;
            case tag_int_array:
                delete value.tag_int_array;
                break;
            case tag_long_array:
                delete value.tag_long_array;
                break;
            default:
                break;
        }
    }
}