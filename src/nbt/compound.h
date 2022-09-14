//
// Created by MrGrim on 8/19/2022.
//

#ifndef MELON_NBT_COMPOUND_H
#define MELON_NBT_COMPOUND_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <memory_resource>
#include <iterator>
#include "nbt.h"
#include "unordered_dense.h"
#include "util/concepts.h"

namespace melon::nbt
{
    class list;

    class compound
    {
    public:
        std::pmr::string *name = nullptr;

        compound() = delete;

        // For building a compound from scratch
        explicit compound(std::string_view name_in = "", int64_t max_size_in = -1, std::unique_ptr<std::pmr::memory_resource> pmr_rsrc_in = nullptr);

        // For parsing a binary NBT buffer
        // This function expects the raw buffer provided to it to be at least 8 bytes larger than the NBT data. The deflate methods in melon::util
        // take care of this.
        explicit compound(std::unique_ptr<char[]> raw, size_t raw_size, std::unique_ptr<std::pmr::memory_resource> pmr_rsrc_in = nullptr);

        template<tag_type_enum tag_type>
        requires is_nbt_container<tag_type>
        std::optional<tag_cont_t<tag_type> *> get(const std::string_view &tag_name) noexcept
        {
            auto itr = tags->find(tag_name);

            if (itr == tags->end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::get<tag_cont_t<tag_type> *>(itr->second);
        }

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        std::optional<std::reference_wrapper<tag_prim_t<tag_type>>> get(const std::string_view &tag_name) noexcept
        {
            auto itr = tags->find(tag_name);

            if (itr == tags->end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::reference_wrapper<tag_prim_t<tag_type>>(std::get<tag_cont_t<tag_type> *>(itr->second)->template get<tag_type>());
        }

        // There is no put for container types. You can get one or create one only.
        template<tag_type_enum tag_type, is_nbt_type_match<tag_type> V>
        requires is_nbt_primitive<tag_type>
        void put(const std::string_view tag_name, V value)
        {
            auto *tag_ptr = get_primitive(tag_name, tag_type);
            std::memcpy(static_cast<void *>(&(tag_ptr->value)), static_cast<void *>(&value), sizeof(V));
        }

        template<tag_type_enum tag_type, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, std::size_t N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type>
        void put(const std::string_view tag_name, const std::array<V, N> &values)
        {
            put_array_general<V>(tag_name, tag_type, values);
        }

        template<tag_type_enum tag_type, template<class, class...> class C = std::initializer_list, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, class... N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type> && is_simple_iterable<C<V, N...>, V>
        void put(const std::string_view tag_name, const C<V, N...> &values)
        {
            put_array_general<V>(tag_name, tag_type, values);
        }

        void to_snbt(std::string &out);

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) = delete;
        compound &operator=(compound &&) = delete;

        ~compound();
    private:
        friend class list;

        explicit compound(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in = nullptr);
        std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>> *init_container();

        primitive_tag *get_primitive(std::string_view, tag_type_enum);
        char *read(char *itr, const char *itr_end);

        template<typename V>
        void put_array_general(const std::string_view tag_name, tag_type_enum tag_type, const auto &values)
        {
            auto *tag_ptr = get_primitive(tag_name, tag_type);

            // This knowingly leaks memory. The top level compound will free the entire pmr_rsrc when it goes out of scope.
            if (tag_ptr->size() < values.size() || tag_ptr->value.generic == 0)
                tag_ptr->value.generic_ptr = pmr_rsrc->allocate(sizeof(V) * values.size(), alignof(V));

            tag_ptr->resize(values.size());
            for (int idx = 0; const auto &value: values)
                static_cast<V *>(tag_ptr->value.generic_ptr)[idx++] = value;
        }

        std::optional<std::variant<compound *, list *>> parent;
        compound                                        *top;
        std::pmr::memory_resource                       *pmr_rsrc;

        std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>> *tags;

        uint16_t depth    = 0;
        size_t   size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_COMPOUND_H
