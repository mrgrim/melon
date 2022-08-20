//
// Created by MrGrim on 8/19/2022.
//

#ifndef MELON_NBT_LIST_H
#define MELON_NBT_LIST_H

#include <cstdint>
#include <string>
#include <vector>
#include "nbt.h"

namespace melon::nbt
{
    class compound;

    class list
    {
    public:
        uint64_t      size  = 0;
        std::string   *name = nullptr;
        tag_type_enum type  = tag_end;
        int32_t       count = 0;

        list() = delete;

        explicit list(std::variant<compound *, list *> parent_in, int64_t max_size_in = -1);
        explicit list(std::vector<uint8_t> *raw_in, std::variant<compound *, list *> parent_in, int64_t max_size_in = -1);

        // I'd honestly prefer these to be private, but that'd require either a custom allocator or an intermediate class
        // that would add temporary objects I'm trying to avoid
        explicit list(std::vector<uint8_t> *raw_in, uint8_t **itr_in, compound *parent_in, bool skip_header = false);
        explicit list(std::vector<uint8_t> *raw_in, uint8_t **itr_in, list *parent_in, bool skip_header = false);

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) noexcept;
        list &operator=(list &&) noexcept;

        ~list();

    private:
        friend class compound;

        uint8_t *read(std::vector<uint8_t> *raw, uint8_t *itr, bool skip_header = false);

        std::vector<primitive_tag> primitives;
        std::vector<list>          lists;
        std::vector<compound>      compounds;

        uint16_t                         depth         = 0;
        uint64_t                         size_tracking = 0;
        int64_t                          max_size      = -1;
        bool                             readonly      = false;
        compound                         *top          = nullptr;
        std::variant<compound *, list *> parent        = (list *)nullptr;
    };
}

#endif //MELON_NBT_LIST_H
