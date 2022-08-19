//
// Created by MrGrim on 8/16/2022.
//

#include <stdlib.h>

#include "nbt/nbt.h"

namespace melon::nbt
{
    nbtcontainer::nbtcontainer(nbtcontainer &&in) noexcept
            : name(in.name), parent(in.parent), size(in.size), depth_v(in.depth_v), max_size(in.max_size), size_tracking(in.size_tracking), readonly(in.readonly)
    {
        in.name   = nullptr;
        in.parent = nullptr;

        in.size          = 0;
        in.depth_v       = 0;
        in.max_size      = -1;
        in.size_tracking = 0;
        in.readonly      = false;
    }

    nbtcontainer &nbtcontainer::operator=(nbtcontainer &&in) noexcept
    {
        if (this != &in)
        {
            delete name;

            name   = in.name;
            parent = in.parent;

            size          = in.size;
            depth_v       = in.depth_v;
            max_size      = in.max_size;
            size_tracking = in.size_tracking;
            readonly      = in.readonly;

            in.name   = nullptr;
            in.parent = nullptr;

            in.size          = 0;
            in.depth_v       = 0;
            in.max_size      = -1;
            in.size_tracking = 0;
            in.readonly      = false;
        }

        return *this;
    }

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
            if (tag_properties[tag_type].is_complex && (void *)(value.tag_byte_array) != nullptr)
                free((void *)(value.tag_byte_array));

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
        //std::cout << "Deleting primitive." << std::endl;
        if (tag_properties[tag_type].is_complex && (void *)(value.tag_string) != nullptr)
            free((void *)(value.tag_byte_array));
    }

    nbtcontainer::~nbtcontainer() {
        delete name;
    }
}