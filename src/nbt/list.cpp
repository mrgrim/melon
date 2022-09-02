//
// Created by MrGrim on 8/14/2022.
//

#include <optional>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <memory_resource>
#include "nbt.h"
#include "compound.h"
#include "list.h"

#if DEBUG == true
#include <iostream>
#endif

namespace melon::nbt
{
    list::list(std::variant<compound *, list *> parent_in, std::string_view name_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              primitives(init_container<primitive_tag *>()),
              lists(init_container<list *>()),
              compounds(init_container<compound *>())
    {
        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        void *ptr = pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string));
        name = new(ptr) std::pmr::string(name_in, pmr_rsrc);
    }

    list::list(list &&in) noexcept
            : name(in.name),
              type(in.type),
              count(in.count),
              parent(in.parent),
              top(in.top),
              pmr_rsrc(in.pmr_rsrc),
              primitives(in.primitives),
              lists(in.lists),
              compounds(in.compounds),
              depth(in.depth),
              size(in.size),
              max_size(in.max_size)
    {
        std::cerr << "!!! Moving list via ctor: " << name << "!" << std::endl;

        in.name   = nullptr;
        in.parent = (list *)nullptr;
        in.top    = nullptr;

        in.type         = tag_end;
        in.size         = 0;
        in.count        = 0;
        in.depth        = 1;
        in.max_size     = -1;
        in.pmr_rsrc     = std::pmr::get_default_resource();
    }

    list &list::operator=(list &&in) noexcept
    {
        if (this != &in)
        {
            std::cerr << "!!! Moving list via operator=: " << in.name << "!" << std::endl;

            name   = in.name;
            parent = in.parent;
            top    = in.top;

            type         = in.type;
            size         = in.size;
            count        = in.count;
            depth        = in.depth;
            max_size     = in.max_size;
            pmr_rsrc     = in.pmr_rsrc;

            primitives = in.primitives;
            compounds  = in.compounds;
            lists      = in.lists;

            in.type  = tag_end;
            in.count = 0;

            in.name   = nullptr;
            in.parent = (list *)nullptr;
            in.top    = nullptr;

            in.type         = tag_end;
            in.size         = 0;
            in.count        = 0;
            in.depth        = 1;
            in.max_size     = -1;
            in.pmr_rsrc     = std::pmr::get_default_resource();
        }

        return *this;
    }

#if NBT_DEBUG == true
    list::~list()
    {
        if (name->empty())
            std::cout << "Deleting anonymous list." << std::endl;
        else
            std::cout << "Deleting list with name " << name << std::endl;
    }
#endif

    list::list(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in, bool no_header)
            : name(name_in),
              parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              primitives(init_container<primitive_tag *>()),
              lists(init_container<list *>()),
              compounds(init_container<compound *>())
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, itr_end, no_header);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    char *list::read(char *itr, const char *itr_end, bool skip_header)
    {
        static_assert(sizeof(count) == sizeof(int32_t));
        static_assert(sizeof(tag_type_enum) == sizeof(char));

        void *ptr;
        auto itr_start = itr;

        uint16_t str_len;
        std::memcpy(static_cast<void *>(&str_len), static_cast<const void *>(itr + 1), sizeof(str_len));
        str_len = cvt_endian(str_len);

        if (name == nullptr)
        {
            ptr = pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string));
            name = new(ptr) std::pmr::string(itr + 3, str_len & (static_cast<int16_t>(skip_header) - 1), pmr_rsrc);
        }

        itr += (str_len + sizeof(str_len) + sizeof(tag_type_enum)) & (static_cast<int16_t>(skip_header) - 1);

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");
        type = tag_type;

        std::memcpy(static_cast<void *>(&count), static_cast<const void *>(itr), sizeof(count));
        count = cvt_endian(count);
        itr += sizeof(count);

        if (tag_properties[tag_type].category == cat_container)
        {
#if NBT_DEBUG == true
            std::cout << "Found a list of " << tag_printable_names[tag_type] << std::endl;
#endif

            if (tag_type == tag_list)
            {
                lists->reserve(count);

                for (int32_t index = 0; index < count; index++)
                {
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous list." << std::endl;
#endif
                    ptr = pmr_rsrc->allocate(sizeof(list), alignof(list));
                    lists->push_back(new(ptr) list(&itr, itr_end, this, nullptr, true));
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous list." << std::endl;
                    compound::lists_parsed++;
#endif
                }
            }
            else if (tag_type == tag_compound)
            {
                compounds->reserve(count);

                for (int32_t index = 0; index < count; index++)
                {
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous compound." << std::endl;
#endif
                    ptr = pmr_rsrc->allocate(sizeof(compound), alignof(compound));
                    compounds->push_back(new(ptr) compound(&itr, itr_end, this, nullptr, true));
#if NBT_DEBUG == true
                    std::cout << "Exiting anonymous compound." << std::endl;
                    compound::compounds_parsed++;
#endif
                }
            }
        }
        else if (tag_properties[tag_type].category == cat_primitive)
        {
#if NBT_DEBUG == true
            std::cout << "Found a list of " << tag_printable_names[tag_type] << " (count: " << count << "): ";
#endif

            primitives->reserve(count);

            for (int32_t index = 0; index < count; index++)
            {
                ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                primitives->push_back(new(ptr) primitive_tag(tag_type, read_tag_primitive(&itr, tag_type)));

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                compound::primitives_parsed++;

                switch (tag_type)
                {
                    case tag_byte:
                        std::cout << +primitives->back()->value.tag_byte << " ";
                        break;
                    case tag_short:
                        std::cout << primitives->back()->value.tag_short << " ";
                        break;
                    case tag_int:
                        std::cout << primitives->back()->value.tag_int << " ";
                        break;
                    case tag_long:
                        std::cout << primitives->back()->value.tag_long << " ";
                        break;
                    case tag_float:
                        std::cout << primitives->back()->value.tag_float << " ";
                        break;
                    case tag_double:
                        std::cout << primitives->back()->value.tag_double << " ";
                        break;
                }
#pragma clang diagnostic pop
#endif
            }

#if NBT_DEBUG == true
            std::cout << std::endl;
#endif
        }
        else if (tag_properties[tag_type].category == cat_array)
        {
            primitives->reserve(count);

            if (tag_type == tag_string)
            {
#if NBT_DEBUG == true
                std::cout << "Found a list of Strings: " << std::endl;
#endif

                for (int32_t index = 0; index < count; index++)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    std::memcpy(&str_len, itr, sizeof(str_len));
                    str_len = cvt_endian(str_len);

                    void *str_ptr = pmr_rsrc->allocate(str_len, alignof(char *));
                    std::memcpy(str_ptr, static_cast<const void *>(itr + sizeof(str_len)), str_len);

                    ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                    primitives->push_back(new(ptr) primitive_tag(tag_type, std::bit_cast<uint64_t>(str_ptr), nullptr, static_cast<uint32_t>(str_len)));

                    itr += sizeof(str_len) + str_len;

#if NBT_DEBUG == true
                    std::cout << std::string_view(primitives->back()->value.tag_string, primitives->back()->size) << std::endl;
                    compound::strings_parsed++;
#endif
                }
            }
            else
            {

#if NBT_DEBUG == true
                std::cout << "Found a list of " << tag_printable_names[tag_type] << " (count: " << count << "): " << std::endl;
#endif

                for (int32_t index = 0; index < count; index++)
                {
                    auto [array_ptr, array_len] = read_tag_array(&itr, tag_type, pmr_rsrc);

                    ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                    primitives->push_back(new(ptr) primitive_tag(tag_type, std::bit_cast<uint64_t>(array_ptr), nullptr, static_cast<uint32_t>(array_len)));

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                    compound::arrays_parsed++;
                    switch (tag_type)
                    {
                        case tag_string:
                            std::cout << std::string_view(primitives->back()->value.tag_string, primitives->back()->size) << std::endl;
                            compound::strings_parsed++;
                            break;
                        case tag_byte_array:
                            for (int array_idx = 0; array_idx < primitives->back()->size; array_idx++)
                                std::cout << +primitives->back()->value.tag_byte_array[array_idx] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
                            break;
                        case tag_int_array:
                            for (int array_idx = 0; array_idx < primitives->back()->size; array_idx++)
                                std::cout << +primitives->back()->value.tag_int_array[array_idx] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
                            break;
                        case tag_long_array:
                            for (int array_idx = 0; array_idx < primitives->back()->size; array_idx++)
                                std::cout << +primitives->back()->value.tag_long_array[array_idx] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
                            break;
                    }
#pragma clang diagnostic pop
#endif
                }
            }
        }

        size = itr - itr_start;
        return itr;
    }

#pragma clang diagnostic pop

}