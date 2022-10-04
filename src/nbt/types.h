//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_TYPES_H
#define MELON_NBT_TYPES_H

#include <string_view>
#include <span>
#include <variant>
#include "constants.h"
#include "util/concepts.h"
#include "util/util.h"

namespace melon::nbt
{
    class list;

    class compound;

    class primitive;

    using tag_primitive_types =
            std::tuple<
                    void,
                    int8_t,
                    int16_t,
                    int32_t,
                    int64_t,
                    float,
                    double,
                    int8_t *,
                    char *,
                    list *,
                    compound *,
                    int32_t *,
                    int64_t *
            >;

    using tag_access_types =
            std::tuple<
                    void,
                    int8_t,
                    int16_t,
                    int32_t,
                    int64_t,
                    float,
                    double,
                    std::span<int8_t>,
                    std::string_view,
                    list,
                    compound,
                    std::span<int32_t>,
                    std::span<int64_t>
            >;

    using tag_container_types =
            std::tuple<
                    void,
                    primitive,
                    primitive,
                    primitive,
                    primitive,
                    primitive,
                    primitive,
                    primitive,
                    primitive,
                    list,
                    compound,
                    primitive,
                    primitive
            >;

    template<tag_type_enum tag_idx>
            requires (tag_idx != tag_end) && (tag_idx < tag_count)
    using tag_prim_t = typename std::tuple_element<tag_idx, tag_primitive_types>::type;

    template<tag_type_enum tag_idx>
            requires (tag_idx != tag_end) && (tag_idx < tag_count)
    using tag_access_t = typename std::tuple_element<tag_idx, tag_access_types>::type;

    template<tag_type_enum tag_idx>
            requires (tag_idx != tag_end) && (tag_idx < tag_count)
    using tag_cont_t = typename std::tuple_element<tag_idx, tag_container_types>::type;

    template<class T>
    struct refwrap_variant_types
    {
    };

    template<std::same_as<void> T>
    struct refwrap_variant_types<T>
    {
        using type = std::monostate;
    };

    template<util::is_contiguous_view T>
    struct refwrap_variant_types<T>
    {
        using type = T;
    };

    template<util::fundamental T>
    requires (!std::is_same_v<void, T>)
    struct refwrap_variant_types<T>
    {
        using type = std::reference_wrapper<std::remove_pointer_t<T>>;
    };

    template<class T>
    concept is_nbt_container_t = std::is_same_v<std::remove_pointer_t<T>, compound> || std::is_same_v<std::remove_pointer_t<T>, list>;

    template<is_nbt_container_t T>
    struct refwrap_variant_types<T>
    {
        using type = std::reference_wrapper<std::remove_pointer_t<T>>;
    };

    template<class T>
    using refwrap_variant_types_t = typename refwrap_variant_types<T>::type;

    using tag_variant_t = util::transform_tuple_types<refwrap_variant_types_t, std::variant, tag_access_types>::type;
}

#endif //MELON_NBT_TYPES_H
