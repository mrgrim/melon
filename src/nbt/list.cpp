//
// Created by MrGrim on 8/14/2022.
//

#include <optional>
#include <stdexcept>
#include <cstring>
#include "nbt.h"
#include "compound.h"
#include "list.h"

#if DEBUG == true
#include <iostream>
#endif

namespace melon::nbt
{
    list::list(std::variant<compound *, list *> parent_in, int64_t max_size_in)
            : depth(1), max_size(max_size_in), parent(parent_in)
    {
        if (std::holds_alternative<list *>(parent))
            top = std::get<list *>(parent)->top;
        else
            top = std::get<compound *>(parent)->top;
    }

    list::list(list &&in) noexcept
            : type(in.type), size(in.size), count(in.count), name(in.name),
              depth(in.depth), size_tracking(in.size_tracking), max_size(in.max_size),
              readonly(in.readonly), parent(in.parent),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        in.name   = nullptr;
        in.parent = (list *)nullptr;

        in.type          = tag_end;
        in.size          = 0;
        in.count         = 0;
        in.depth         = 1;
        in.size_tracking = 0;
        in.max_size      = -1;
        in.readonly      = false;
    }

    list &list::operator=(list &&in) noexcept
    {
        if (this != &in)
        {
            delete (name);

            name   = in.name;
            parent = in.parent;

            type          = in.type;
            size          = in.size;
            count         = in.count;
            depth         = in.depth;
            size_tracking = in.size_tracking;
            max_size      = in.max_size;
            readonly      = in.readonly;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.type  = tag_end;
            in.count = 0;

            in.name   = nullptr;
            in.parent = (list *)nullptr;

            in.type          = tag_end;
            in.size          = 0;
            in.count         = 0;
            in.depth         = 1;
            in.size_tracking = 0;
            in.max_size      = -1;
            in.readonly      = false;
        }

        return *this;
    }

    list::~list()
    {
#if NBT_DEBUG == true
        if (name == nullptr)
            std::cout << "Deleting anonymous list." << std::endl;
        else
            std::cout << "Deleting list with name " << *name << std::endl;
#endif
        delete name;
    }

    list::list(uint8_t **itr_in, compound *parent_in, bool skip_header)
            : depth(parent_in->depth + 1), max_size(parent_in->max_size), size_tracking(parent_in->size_tracking), parent(parent_in), top(parent_in->top)
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }

    list::list(uint8_t **itr_in, list *parent_in, bool skip_header)
            : depth(parent_in->depth + 1), max_size(parent_in->max_size), size_tracking(parent_in->size_tracking), parent(parent_in), top(parent_in->top)
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }


#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

    uint8_t *list::read(uint8_t *itr, bool skip_header)
    {
        if (itr == nullptr) throw std::runtime_error("NBT List Read called with NULL iterator.");

        auto itr_start = itr;

        if (!skip_header)
        {
            if (static_cast<tag_type_enum>(*itr) != tag_list) throw std::runtime_error("NBT Tag Type Not List.");
            *itr++ = 0; // This has the side effect of null terminating any strings if the parent object was a list of strings.

            auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            delete name;
            name = new std::string(reinterpret_cast<char *>(itr), name_len);
            itr += name_len;
        }

        auto tag_type = static_cast<tag_type_enum>(*itr);
        if (tag_type >= tag_properties.size()) throw std::runtime_error("Invalid NBT Tag Type.");
        type = tag_type;

        *itr++ = 0; // This has the side effect of null terminating any strings if the parent object was a list of strings.

        count = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
        itr += 4;

        if (tag_properties[tag_type].size == 127)
        {
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
#endif
                }
            }
        }
        else if (tag_properties[tag_type].size > 0)
        {
            primitives.reserve(count);

            // C++14 guarantees the start address is the same across union members making a memcpy safe to do
            for (int32_t index = 0; index < count; index++)
            {
                uint64_t prim_value;
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

                primitives.emplace_back(tag_type, prim_value);
                itr += tag_properties[tag_type].size;
            }
        }
        else if (tag_properties[tag_type].size < 0)
        {
            primitives.reserve(count);

            if (tag_type == tag_string)
            {
                // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                auto str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                itr += 2;

                primitives.emplace_back(tag_type, (uint64_t)itr);
                itr += str_len;

                for (int32_t index = 1; index < count; index++)
                {
                    // We do a sneaky thing here to avoid allocating memory. Pull the next iterations size here to make room for the null termination.
                    // Don't do this for the last string. The parent compound processing will replace the type with 0 which will null terminate the
                    // last string in this list.
                    str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                    *itr = 0;
                    itr += 2;

                    primitives.emplace_back(tag_type, (uint64_t)itr);
                    itr += str_len;
                }
            }
            else
            {
                for (int32_t index = 0; index < count; index++)
                {
                    auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

                    switch (tag_type)
                    {
                        case tag_byte_array:
                            cvt_endian_array<int8_t>(array_len, reinterpret_cast<int8_t *>(itr));
                            break;
                        case tag_int_array:
                            cvt_endian_array<int32_t>(array_len, reinterpret_cast<int32_t *>(itr));
                            break;
                        case tag_long_array:
                            cvt_endian_array<int64_t>(array_len, reinterpret_cast<int64_t *>(itr));
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    primitives.emplace_back(tag_type, (uint64_t)itr);
                    itr += array_len * (tag_properties[tag_type].size * -1);
                }
            }
        }

        size = itr - itr_start;
        return itr;
    }

#pragma clang diagnostic pop
}