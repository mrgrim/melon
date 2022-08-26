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
    list::list(std::variant<compound *, list *> parent_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              primitives(pmr_rsrc),
              compounds(pmr_rsrc),
              lists(pmr_rsrc)
    {
        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);
    }

    list::list(list &&in) noexcept
            : type(in.type),
              size(in.size),
              count(in.count),
              name(in.name),
              depth(in.depth),
              max_size(in.max_size),
              parent(in.parent),
              top(in.top),
              pmr_rsrc(in.pmr_rsrc),
              name_backing(in.name_backing),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        std::cerr << "!!! Moving list via ctor: " << name << "!" << std::endl;

        in.name   = std::string_view();
        in.parent = (list *)nullptr;
        in.top    = nullptr;

        in.type         = tag_end;
        in.size         = 0;
        in.count        = 0;
        in.depth        = 1;
        in.max_size     = -1;
        in.name_backing = nullptr;
        in.pmr_rsrc     = std::pmr::get_default_resource();
    }

    list &list::operator=(list &&in) noexcept
    {
        if (this != &in)
        {
            std::cerr << "!!! Moving list via operator=: " << in.name << "!" << std::endl;

            delete (name_backing);

            name   = in.name;
            parent = in.parent;
            top    = in.top;

            type         = in.type;
            size         = in.size;
            count        = in.count;
            depth        = in.depth;
            max_size     = in.max_size;
            name_backing = in.name_backing;
            pmr_rsrc     = in.pmr_rsrc;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.type  = tag_end;
            in.count = 0;

            in.name   = std::string_view();
            in.parent = (list *)nullptr;
            in.top    = nullptr;

            in.type         = tag_end;
            in.size         = 0;
            in.count        = 0;
            in.depth        = 1;
            in.max_size     = -1;
            in.name_backing = nullptr;
            in.pmr_rsrc     = std::pmr::get_default_resource();
        }

        return *this;
    }

    list::~list()
    {
#if NBT_DEBUG == true
        if (name.empty())
            std::cout << "Deleting anonymous list." << std::endl;
        else
            std::cout << "Deleting list with name " << name << std::endl;
#endif
        delete name_backing;
    }

    list::list(std::byte **itr_in, std::variant<compound *, list *> parent_in, bool skip_header)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              primitives(pmr_rsrc),
              compounds(pmr_rsrc),
              lists(pmr_rsrc)
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, skip_header);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    std::byte *list::read(std::byte *itr, bool skip_header)
    {
        static_assert(sizeof(count) == sizeof(int32_t));
        static_assert(sizeof(tag_type_enum) == sizeof(std::byte));

        if (itr == nullptr) [[unlikely]] throw std::runtime_error("NBT List Read called with NULL iterator.");

        auto itr_start = itr;

        if (static_cast<tag_type_enum>(*itr) != tag_list) [[unlikely]] throw std::runtime_error("NBT Tag Type Not List.");

        uint16_t str_len;
        std::memcpy(&str_len, itr + 1, sizeof(str_len));
        str_len = cvt_endian(str_len);

        name = std::string_view(reinterpret_cast<char *>(itr + 3), str_len & (static_cast<int16_t>(skip_header) - 1));

        itr += (str_len + sizeof(str_len) + sizeof(tag_type_enum)) & (static_cast<int16_t>(skip_header) - 1);

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");
        type = tag_type;

        std::memcpy(&count, itr, sizeof(count));
        count = cvt_endian(count);
        itr += sizeof(count);

        if (tag_properties[tag_type].category == cat_complex)
        {
#if NBT_DEBUG == true
            std::cout << "Found a list of " << tag_printable_names[tag_type] << std::endl;
#endif

            if (tag_type == tag_list)
            {
                lists.reserve(count);

                for (int32_t index = 0; index < count; index++)
                {
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous list." << std::endl;
#endif
                    lists.emplace_back(&itr, this);
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous list." << std::endl;
                    compound::lists_parsed++;
#endif
                }
            }
            else if (tag_type == tag_compound)
            {
                compounds.reserve(count);

                for (int32_t index = 0; index < count; index++)
                {
#if NBT_DEBUG == true
                    std::cout << "Entering anonymous compound." << std::endl;
#endif
                    compounds.emplace_back(&itr, this, true);
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
            std::cout << "Found a list of " << tag_printable_names[tag_type] << ": ";
#endif

            primitives.reserve(count);

            for (int32_t index = 0; index < count; index++)
            {
                const primitive_tag &result = primitives.emplace_back(tag_type, read_tag_primitive(&itr, tag_type));

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                compound::primitives_parsed++;

                switch (tag_type)
                {
                    case tag_byte:
                        std::cout << +result.value.tag_byte << " ";
                        break;
                    case tag_short:
                        std::cout << result.value.tag_short << " ";
                        break;
                    case tag_int:
                        std::cout << result.value.tag_int << " ";
                        break;
                    case tag_long:
                        std::cout << result.value.tag_long << " ";
                        break;
                    case tag_float:
                        std::cout << result.value.tag_float << " ";
                        break;
                    case tag_double:
                        std::cout << result.value.tag_double << " ";
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
            primitives.reserve(count);

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
                    itr += sizeof(str_len);

#if NBT_DEBUG == true
                    std::cout << std::string_view(reinterpret_cast<char *>(itr), str_len) << std::endl;
                    compound::strings_parsed++;
#endif

                    primitives.emplace_back(tag_type, reinterpret_cast<uint64_t>(itr), static_cast<uint32_t>(str_len));
                    itr += str_len;
                }
            }
            else
            {
#if NBT_DEBUG == true
                std::cout << "Found a list of " << tag_printable_names[tag_type] << ": " << std::endl;
#endif

                for (int32_t index = 0; index < count; index++)
                {
                    auto [array_ptr, array_len] = read_tag_array(&itr, tag_type, pmr_rsrc);

#if NBT_DEBUG == true
                    const primitive_tag &result = primitives.emplace_back(tag_type, reinterpret_cast<uint64_t>(array_ptr), static_cast<uint32_t>(array_len));
                    compound::arrays_parsed++;
#else
                    primitives.emplace_back(tag_type, reinterpret_cast<uint64_t>(array_ptr), static_cast<uint32_t>(array_len));
#endif

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                    switch (tag_type)
                    {
                        case tag_byte_array:
                            for (int array_idx = 0; array_idx < array_len; array_idx++)
                                std::cout << +result.value.tag_byte_array[array_idx] << " ";
                            std::cout << std::endl;
                            break;
                        case tag_int_array:
                            for (int array_idx = 0; array_idx < array_len; array_idx++)
                                std::cout << +result.value.tag_int_array[array_idx] << " ";
                            break;
                        case tag_long_array:
                            for (int array_idx = 0; array_idx < array_len; array_idx++)
                                std::cout << +result.value.tag_long_array[array_idx] << " ";
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