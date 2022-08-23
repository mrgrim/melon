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
              top(extract_top_compound()),
              pmr_rsrc(top->pmr_rsrc),
              primitives(extract_pmr_rsrc()),
              compounds(extract_pmr_rsrc()),
              lists(extract_pmr_rsrc())
    {
        if (std::holds_alternative<list *>(parent))
        {
            auto parent_l = std::get<list *>(parent);

            depth         = parent_l->depth + 1;
            max_size      = parent_l->max_size + 1;
            size_tracking = parent_l->size_tracking;
            readonly      = parent_l->readonly;
        }
        else
        {
            auto parent_l = std::get<compound *>(parent);

            depth         = parent_l->depth + 1;
            max_size      = parent_l->max_size + 1;
            size_tracking = parent_l->size_tracking;
            readonly      = parent_l->readonly;
        }
    }

    list::list(list &&in) noexcept
            : type(in.type),
              size(in.size),
              count(in.count),
              name(in.name),
              depth(in.depth),
              size_tracking(in.size_tracking),
              max_size(in.max_size),
              readonly(in.readonly),
              parent(in.parent),
              top(in.top),
              pmr_rsrc(get_std_default_pmr_rsrc()),
              name_backing(in.name_backing),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        in.name   = std::string_view();
        in.parent = (list *)nullptr;

        in.type          = tag_end;
        in.size          = 0;
        in.count         = 0;
        in.depth         = 1;
        in.size_tracking = 0;
        in.max_size      = -1;
        in.readonly      = false;
        in.name_backing  = nullptr;
    }

    list &list::operator=(list &&in) noexcept
    {
        if (this != &in)
        {
            delete (name_backing);

            name   = in.name;
            parent = in.parent;

            type          = in.type;
            size          = in.size;
            count         = in.count;
            depth         = in.depth;
            size_tracking = in.size_tracking;
            max_size      = in.max_size;
            readonly      = in.readonly;
            name_backing  = in.name_backing;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.type  = tag_end;
            in.count = 0;

            in.name   = std::string_view();
            in.parent = (list *)nullptr;

            in.type          = tag_end;
            in.size          = 0;
            in.count         = 0;
            in.depth         = 1;
            in.size_tracking = 0;
            in.max_size      = -1;
            in.readonly      = false;
            in.name_backing  = nullptr;
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

    list::list(std::byte **itr_in, compound *parent_in, bool skip_header)
            : depth(parent_in->depth + 1),
              max_size(parent_in->max_size),
              size_tracking(parent_in->size_tracking),
              parent(parent_in),
              top(parent_in->top),
              pmr_rsrc(top->pmr_rsrc),
              primitives(extract_pmr_rsrc()),
              compounds(extract_pmr_rsrc()),
              lists(extract_pmr_rsrc())
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }

    list::list(std::byte **itr_in, list *parent_in, bool skip_header)
            : depth(parent_in->depth + 1),
              max_size(parent_in->max_size),
              size_tracking(parent_in->size_tracking),
              parent(parent_in),
              top(parent_in->top),
              pmr_rsrc(top->pmr_rsrc),
              primitives(extract_pmr_rsrc()),
              compounds(extract_pmr_rsrc()),
              lists(extract_pmr_rsrc())
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }


#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    std::byte *list::read(std::byte *itr, bool skip_header)
    {
        if (itr == nullptr) throw std::runtime_error("NBT List Read called with NULL iterator.");

        auto itr_start = itr;

        if (!skip_header)
        {
            if (static_cast<tag_type_enum>(*itr++) != tag_list) throw std::runtime_error("NBT Tag Type Not List.");

            auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            name = std::string_view(reinterpret_cast<char *>(itr), name_len);
            itr += name_len;
        }

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) throw std::runtime_error("Invalid NBT Tag Type.");
        type = tag_type;

        count = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
        itr += 4;

        if (tag_properties[tag_type].size == 127)
        {
#if NBT_DEBUG == true
            std::cout << "Found a list of " << tag_properties[tag_type].name << std::endl;
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
        else if (tag_properties[tag_type].size > 0)
        {
#if NBT_DEBUG == true
            std::cout << "Found a list of " << tag_properties[tag_type].name << ": ";
#endif

            primitives.reserve(count);

            for (int32_t index = 0; index < count; index++)
            {
                uint64_t prim_value;
                // C++14 guarantees the start address is the same across union members making a memcpy safe to do
                std::memcpy(reinterpret_cast<void *>(&prim_value), reinterpret_cast<void *>(itr), sizeof(prim_value));

                // Change endianness based on type size to keep it to these 3 cases. The following code should compile
                // away on big endian systems.
                switch (tag_properties[tag_type].size)
                {
                    case 2:
                        *(reinterpret_cast<uint16_t *>(&prim_value)) = cvt_endian(*(reinterpret_cast<uint16_t *>(&prim_value)));
                        break;
                    case 4:
                        *(reinterpret_cast<uint32_t *>(&prim_value)) = cvt_endian(*(reinterpret_cast<uint32_t *>(&prim_value)));
                        break;
                    case 8:
                        prim_value = cvt_endian(prim_value);
                        break;
                }

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                compound::primitives_parsed++;

                switch (tag_type)
                {
                    case tag_byte:
                        std::cout << +(*((int8_t *)(&prim_value))) << " ";
                        break;
                    case tag_short:
                        std::cout << (*((int16_t *)(&prim_value))) << " ";
                        break;
                    case tag_int:
                        std::cout << (*((int32_t *)(&prim_value))) << " ";
                        break;
                    case tag_long:
                        std::cout << (*((int64_t *)(&prim_value))) << " ";
                        break;
                    case tag_float:
                        std::cout << (*((float *)(&prim_value))) << " ";
                        break;
                    case tag_double:
                        std::cout << (*((double *)(&prim_value))) << " ";
                        break;
                }
#pragma clang diagnostic pop
#endif

                primitives.emplace_back(tag_type, prim_value);
                itr += tag_properties[tag_type].size;
            }

#if NBT_DEBUG == true
            std::cout << std::endl;
#endif
        }
        else if (tag_properties[tag_type].size < 0)
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
                    auto str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                    itr += 2;

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
                std::cout << "Found a list of " << tag_properties[tag_type].name << ": " << std::endl;
#endif

                for (int32_t index = 0; index < count; index++)
                {
                    auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

                    switch (tag_type)
                    {
                        case tag_byte_array:
                            cvt_endian_array<int8_t>(array_len, reinterpret_cast<int8_t *>(itr));
#if NBT_DEBUG == true
                            for (int index_a = 0; index_a < array_len; index_a++) std::cout << +((int8_t *)(itr))[index_a] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
#endif
                            break;
                        case tag_int_array:
                            cvt_endian_array<int32_t>(array_len, reinterpret_cast<int32_t *>(itr));
#if NBT_DEBUG == true
                            for (int index_a = 0; index_a < array_len; index_a++) std::cout << +((int32_t *)(itr))[index_a] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
#endif
                            break;
                        case tag_long_array:
                            cvt_endian_array<int64_t>(array_len, reinterpret_cast<int64_t *>(itr));
#if NBT_DEBUG == true
                            for (int index_a = 0; index_a < array_len; index_a++) std::cout << +((int64_t *)(itr))[index_a] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
#endif
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    primitives.emplace_back(tag_type, (uint64_t)itr, static_cast<uint32_t>(array_len));
                    itr += array_len * (tag_properties[tag_type].size * -1);
                }
            }
        }

        size = itr - itr_start;
        return itr;
    }

#pragma clang diagnostic pop

    const compound *list::extract_top_compound()
    {
        if (std::holds_alternative<compound *>(parent))
            return std::get<compound *>(parent)->top;
        else
            return std::get<list *>(parent)->top;
    }

    std::pmr::memory_resource *list::extract_pmr_rsrc()
    {
        if (std::holds_alternative<std::shared_ptr<std::pmr::memory_resource>>(pmr_rsrc))
            return std::get<std::shared_ptr<std::pmr::memory_resource>>(pmr_rsrc).get();
        else
            return std::get<std::pmr::memory_resource *>(pmr_rsrc);
    }

}