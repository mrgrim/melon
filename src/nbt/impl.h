//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_IMPL_H
#define MELON_NBT_IMPL_H

#include <optional>
#include <cstring>

namespace melon::nbt::impl
{
    struct container_property_args : util::forced_named_init<container_property_args>
    {
        std::optional<uint16_t> new_depth = std::nullopt;
        std::optional<int64_t> new_max_bytes = std::nullopt;
        std::optional<std::variant<compound *, list *>> new_parent = std::nullopt;
        std::optional<compound *> new_top = std::nullopt;
    };

    template<class T>
    T read_var(char *&itr)
    {
        T var;
        std::memcpy(&var, itr, sizeof(T));
        itr += sizeof(T);
        return util::cvt_endian(var);
    }

    uint64_t inline
#ifdef __GNUC__
    __attribute__((always_inline))
#endif
    read_tag_primitive(char **itr, tag_type_enum tag_type) noexcept
    {
        uint64_t prim_value;

        // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
        std::memcpy(static_cast<void *>(&prim_value), static_cast<const void *>(*itr), sizeof(prim_value));
        prim_value = util::cvt_endian(prim_value);

        // Pack the value to the left so when read from the union with the proper type it will be correct.
        prim_value = util::pack_left(prim_value, tag_properties[tag_type].size);

        *itr += tag_properties[tag_type].size;

        return prim_value;
    }

    std::tuple<std::unique_ptr<char[], mem::pmr::generic_deleter<char[]>>, int32_t>
    inline
#ifdef __GNUC__
    __attribute__((always_inline))
#endif
    read_tag_array(char **itr, const char *const itr_end, tag_type_enum tag_type, std::pmr::memory_resource *pmr_rsrc)
    {
        auto array_len = read_var<int32_t>(*itr);

        if (array_len < 0) [[unlikely]] throw std::runtime_error("Found array with negative length while parsing binary NBT data.");

        if ((*itr + (array_len * tag_properties[tag_type].size) + padding_size) >= itr_end)
            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

        auto array_size  = array_len * tag_properties[tag_type].size + padding_size;
        auto array_align = tag_properties[tag_type].size;
        auto array_ptr   = static_cast<char *>(pmr_rsrc->allocate(array_size, array_align));

        auto array_uptr = std::unique_ptr<char[], mem::pmr::generic_deleter<char[]>>
        (array_ptr, mem::pmr::generic_deleter<char[]>(pmr_rsrc, array_size, array_align));

        // My take on a branchless conversion of an unaligned big endian array of an arbitrarily sized data type to an aligned little endian array.
        for (auto array_idx = 0; array_idx < array_len; array_idx++)
        {
            uint64_t prim_value = read_tag_primitive(itr, tag_type);
            std::memcpy(static_cast<void *>(array_ptr), static_cast<const void *>(&prim_value), sizeof(prim_value));

            array_ptr += tag_properties[tag_type].size;
        }

        return std::make_tuple(std::move(array_uptr), array_len);
    }

    std::tuple<std::unique_ptr<char[], mem::pmr::array_deleter<char[]>>, uint16_t>
    inline
#ifdef __GNUC__
    __attribute__((always_inline))
#endif
    read_tag_string(char **itr, const char *const itr_end, std::pmr::memory_resource *pmr_rsrc)
    {
        // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
        // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
        auto str_len = read_var<uint16_t>(*itr);

        if ((*itr + str_len + padding_size) >= itr_end)
            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

        auto str_ptr = mem::pmr::make_unique<char[]>(pmr_rsrc, str_len + padding_size);
        std::memcpy(str_ptr.get(), *itr, str_len);
        *itr += str_len;

        return std::make_tuple(std::move(str_ptr), str_len);
    }
}

#endif //MELON_NBT_IMPL_H
