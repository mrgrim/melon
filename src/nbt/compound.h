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
    private:
        // We put this up here so it's the last thing to be destructed. This backs all the string_view's in the entire structure.
        std::unique_ptr<std::vector<std::byte>> raw = nullptr;

    public:
        std::string_view name;

        compound() = delete;

        explicit compound(std::optional<std::variant<compound *, list *>> parent_in = std::nullopt, int64_t max_size_in = -1,
                          std::pmr::memory_resource *pmr_rsrc_in = std::pmr::get_default_resource());
        explicit compound(std::unique_ptr<std::vector<std::byte>> raw_in, std::pmr::memory_resource *pmr_rsrc_in = std::pmr::get_default_resource());

        // I'd honestly prefer these to be private, but that'd require either a custom allocator or an intermediate class
        // that would add temporary objects I'm trying to avoid
        explicit compound(std::byte **itr_in, std::variant<compound *, list *>, bool skip_header = false);

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) noexcept;
        compound &operator=(compound &&) noexcept;

        ~compound();

#if NBT_DEBUG == true
        inline static uint32_t compounds_parsed;
        inline static uint32_t lists_parsed;
        inline static uint32_t primitives_parsed;
        inline static uint32_t strings_parsed;
        inline static uint32_t arrays_parsed;
#endif
    private:
        friend class list;

        std::byte *read(std::byte *itr, bool skip_header = false);

        std::optional<std::variant<compound *, list *>> parent    = std::nullopt;
        compound                                        *top;
        std::pmr::memory_resource                       *pmr_rsrc = std::pmr::get_default_resource();

        std::pmr::unordered_map<std::string_view, primitive_tag> primitives;
        std::pmr::unordered_map<std::string_view, compound>      compounds;
        std::pmr::unordered_map<std::string_view, list>          lists;

        std::pmr::string *name_backing = nullptr;

        uint16_t depth    = 0;
        uint64_t size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_COMPOUND_H
