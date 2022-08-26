//
// Created by MrGrim on 8/19/2022.
//

#ifndef MELON_NBT_LIST_H
#define MELON_NBT_LIST_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <memory_resource>
#include "nbt.h"

namespace melon::nbt
{
    class compound;

    class list
    {
    public:
        std::string_view name;
        tag_type_enum    type  = tag_end;
        int32_t          count = 0;

        list() = delete;

        explicit list(std::variant<compound *, list *> parent_in);

        // I'd honestly prefer these to be private, but that'd require either a custom allocator or an intermediate class
        // that would add temporary objects I'm trying to avoid
        explicit list(std::byte **itr_in, std::variant<compound *, list *>, bool skip_header = false);

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) noexcept;
        list &operator=(list &&) noexcept;

        ~list();

    private:
        friend class compound;

        std::byte *read(std::byte *itr, bool skip_header = false);

        std::variant<compound *, list *> parent;
        compound                         *top;
        std::pmr::memory_resource        *pmr_rsrc = std::pmr::get_default_resource();

        std::pmr::vector<primitive_tag> primitives;
        std::pmr::vector<list>          lists;
        std::pmr::vector<compound>      compounds;

        std::pmr::string *name_backing = nullptr;

        uint16_t depth    = 0;
        uint64_t size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_LIST_H
