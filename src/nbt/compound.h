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
#include "nbt.h"

namespace melon::nbt
{
    class list;

    class compound
    {
    public:
        uint64_t         size = 0;
        std::string_view name;

        compound() = delete;

        explicit compound(std::optional<std::variant<compound *, list *>> parent_in = std::nullopt, int64_t max_size_in = -1);
        explicit compound(std::unique_ptr<std::vector<uint8_t>> raw_in, int64_t max_size_in = -1);

        // I'd honestly prefer these to be private, but that'd require either a custom allocator or an intermediate class
        // that would add temporary objects I'm trying to avoid
        explicit compound(uint8_t **itr_in, compound *parent_in, bool skip_header = false);
        explicit compound(uint8_t **itr_in, list *parent_in, bool skip_header = false);

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) noexcept;
        compound &operator=(compound &&) noexcept;

        ~compound();
    private:
        friend class list;

        uint8_t *read(uint8_t *itr = nullptr, bool skip_header = false);

        std::unordered_map<std::string_view, primitive_tag> primitives;
        std::unordered_map<std::string_view, compound>      compounds;
        std::unordered_map<std::string_view, list>          lists;

        uint16_t    depth         = 0;
        uint64_t    size_tracking = 0;
        int64_t     max_size      = -1;
        bool        readonly      = false;
        compound    *top          = nullptr;
        std::string *name_backing = nullptr;

        std::variant<compound *, list *>      parent = (compound *)nullptr;
        std::unique_ptr<std::vector<uint8_t>> raw = nullptr;
    };
}

#endif //MELON_NBT_COMPOUND_H
