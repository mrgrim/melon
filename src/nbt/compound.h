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
#include "nbt.h"
#include "unordered_dense.h"

namespace melon::nbt
{
    class list;

    class compound
    {
    public:
        std::pmr::string *name = nullptr;

        compound() = delete;

        // For building a compound from scratch
        explicit compound(std::string_view name_in = "", int64_t max_size_in = -1, std::pmr::memory_resource *upstream_pmr_rsrc = std::pmr::get_default_resource());

        // For parsing a binary NBT buffer
        explicit compound(std::unique_ptr<std::vector<std::byte>> raw_in, std::pmr::memory_resource *pmr_rsrc_in = std::pmr::get_default_resource());

        template<tag_type_enum tag_idx, class V> requires(is_nbt_primitive<tag_idx> && std::same_as<V, tag_prim_t<tag_idx>>)
        void put(std::string_view tag_name, V value)
        {
            auto itr = primitives->find(tag_name);
            uint64_t generic_value = pack_left(*((uint64_t *)(&value)), sizeof(tag_prim_t<tag_idx>));

            if (itr == primitives->end())
            {
                auto *str_ptr = reinterpret_cast<std::pmr::string *>(pmr_rsrc->allocate(sizeof(std::pmr::string)));
                auto *tag_ptr = reinterpret_cast<primitive_tag *>(pmr_rsrc->allocate(sizeof(primitive_tag)));

                str_ptr = new(str_ptr) std::pmr::string(tag_name, pmr_rsrc);
                tag_ptr = new(tag_ptr) primitive_tag(tag_idx, generic_value, str_ptr);

                (*primitives)[*str_ptr] = tag_ptr;
            }
            else
            {
                itr->second->value.generic = generic_value;
            }
        }

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) noexcept;
        compound &operator=(compound &&) noexcept;

#if NBT_DEBUG == true
        ~compound();

        inline static uint32_t compounds_parsed;
        inline static uint32_t lists_parsed;
        inline static uint32_t primitives_parsed;
        inline static uint32_t strings_parsed;
        inline static uint32_t arrays_parsed;
#else
        ~compound() = default;
#endif
    private:
        friend class list;

        explicit compound(std::byte **itr_in, const std::byte *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in = nullptr, bool no_header = false);

        template<typename T>
        std::pmr::unordered_map<std::string_view, T> *init_container()
        {
            void *ptr;

            ptr = pmr_rsrc->allocate(sizeof(std::pmr::unordered_map<std::string_view, T>));
            return new(ptr) std::pmr::unordered_map<std::string_view, T>(pmr_rsrc);
        }

        std::byte *read(std::byte *itr, const std::byte *itr_end, bool skip_header = false);

        std::optional<std::variant<compound *, list *>> parent;
        compound                                        *top;
        std::pmr::memory_resource                       *pmr_rsrc;

        std::pmr::unordered_map<std::string_view, primitive_tag *> *primitives;
        std::pmr::unordered_map<std::string_view, compound *>      *compounds;
        std::pmr::unordered_map<std::string_view, list *>          *lists;

        uint16_t depth    = 0;
        size_t   size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_COMPOUND_H
