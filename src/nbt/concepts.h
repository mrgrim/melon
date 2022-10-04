//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_CONCEPTS_H
#define MELON_NBT_CONCEPTS_H

#include "constants.h"
#include "types.h"

namespace melon::nbt
{
    class list;

    class compound;

    template<tag_type_enum tag_idx>
    concept is_nbt_primitive = (tag_idx < tag_count) && (tag_properties[tag_idx].category == tag_category_enum::cat_primitive);

    template<tag_type_enum tag_idx>
    concept is_nbt_array = (tag_idx < tag_count) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_array | tag_category_enum::cat_string)) != 0);

    template<tag_type_enum tag_idx>
    concept is_nbt_container = (tag_idx < tag_count) && ((tag_properties[tag_idx].category & (tag_category_enum::cat_list | tag_category_enum::cat_compound)) != 0);

    template<typename T, tag_type_enum tag_type>
    concept is_nbt_type_match = std::is_same_v<tag_prim_t<tag_type>, std::remove_reference_t<T>>;
}

#endif //MELON_NBT_CONCEPTS_H
