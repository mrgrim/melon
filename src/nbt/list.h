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
        std::pmr::string *name = nullptr;
        tag_type_enum    type  = tag_end;
        int32_t          count = 0;

        list() = delete;

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) noexcept;
        list &operator=(list &&) noexcept;

#if NBT_DEBUG == true
        ~list();
#else
        ~list() = default;
#endif

    private:
        friend class compound;

        explicit list(std::variant<compound *, list *> parent_in, std::string_view name_in);
        explicit list(std::byte **itr_in, const std::byte *itr_end, std::variant<compound *, list *>, std::pmr::string *name_in, bool no_header = false);

        template<typename T>
        std::pmr::vector<T> *init_container()
        {
            void *ptr;

            ptr = pmr_rsrc->allocate(sizeof(std::pmr::vector<T>));
            return new(ptr) std::pmr::vector<T>(pmr_rsrc);
        }

        std::byte *read(std::byte *itr, const std::byte *itr_end, bool skip_header = false);

        std::variant<compound *, list *> parent;
        compound                         *top;
        std::pmr::memory_resource        *pmr_rsrc = std::pmr::get_default_resource();

        std::pmr::vector<primitive_tag *> *primitives;
        std::pmr::vector<list *>          *lists;
        std::pmr::vector<compound *>      *compounds;

        uint16_t depth    = 0;
        uint64_t size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_LIST_H
