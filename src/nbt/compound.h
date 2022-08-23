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
        uint64_t         size = 0;
        std::string_view name;

        compound() = delete;

        explicit compound(std::optional<std::variant<compound *, list *>> parent_in = std::nullopt, int64_t max_size_in = -1, std::byte *pmr_buf = nullptr,
                          int64_t pmr_buf_suze = 8632);
        explicit compound(std::unique_ptr<std::vector<std::byte>> raw_in, std::byte *pmr_buf = nullptr, int64_t pmr_buf_suze = 8632);

        // I'd honestly prefer these to be private, but that'd require either a custom allocator or an intermediate class
        // that would add temporary objects I'm trying to avoid
        explicit compound(std::byte **itr_in, compound *parent_in, bool skip_header = false);
        explicit compound(std::byte **itr_in, list *parent_in, bool skip_header = false);

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

        const compound *extract_top_compound();
        std::variant<std::pmr::memory_resource *, std::shared_ptr<std::pmr::memory_resource>> construct_pmr_rsrc(std::byte *pmr_buf, int64_t pmr_buf_size) const;
        std::pmr::memory_resource *extract_pmr_rsrc();

        std::optional<std::variant<compound *, list *>>     parent = std::nullopt;
        const compound                                      *top;
        std::variant<std::pmr::memory_resource *,
                std::shared_ptr<std::pmr::memory_resource>> pmr_rsrc;

        std::pmr::unordered_map<std::string_view, primitive_tag> primitives;
        std::pmr::unordered_map<std::string_view, compound>      compounds;
        std::pmr::unordered_map<std::string_view, list>          lists;

        uint16_t    depth         = 0;
        uint64_t    size_tracking = 0;
        int64_t     max_size      = -1;
        bool        readonly      = false;
        std::string *name_backing = nullptr;
    };
}

#endif //MELON_NBT_COMPOUND_H
