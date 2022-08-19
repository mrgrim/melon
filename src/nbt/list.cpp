//
// Created by MrGrim on 8/14/2022.
//

#include <optional>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include "nbt.h"

namespace melon::nbt
{

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-use-after-move"
    list::list(list &&in) noexcept
            : nbtcontainer(std::move(in)),
              type(in.type), count(in.count),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        in.type          = tag_end;
        in.count         = 0;
    }
#pragma clang diagnostic pop

    list &list::operator=(list &&in) noexcept
    {
        if (this != &in)
        {
            nbtcontainer::operator=(std::move(*this));

            type          = in.type;
            count         = in.count;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.type          = tag_end;
            in.count         = 0;
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
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

    uint8_t *list::read(std::vector<uint8_t> *raw, uint8_t *itr, bool skip_header)
    {
        if (itr == nullptr) throw std::runtime_error("NBT List Read called with NULL iterator.");

        auto itr_start = itr;

        if (!skip_header)
        {
            if (raw->size() < 8) throw std::runtime_error("NBT List Tag Too Small.");
            if (static_cast<tag_type_enum>(*itr++) != tag_list) throw std::runtime_error("NBT Tag Type Not List.");

            auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            delete name;
            name = new std::string(reinterpret_cast<char *>(itr), name_len);
            itr += name_len;
        }

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) throw std::runtime_error("Invalid NBT Tag Type.");
        type = tag_type;

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
                    lists.emplace_back(raw, &itr, static_cast<nbtcontainer *>(this));
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
                    compounds.emplace_back(raw, &itr, static_cast<nbtcontainer *>(this), true);
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
                for (int32_t index = 0; index < count; index++)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    auto str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                    itr += 2;

                    auto str = (char *)malloc(str_len + 1);
                    std::memcpy((void *)str, (void *)itr, str_len);
                    str[str_len] = 0;

                    primitives.emplace_back(tag_type, (uint64_t)str);
                    itr += str_len;
                }
            }
            else
            {
                for (int32_t index = 0; index < count; index++)
                {
                    auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

                    void *array_ptr;

                    switch (tag_type)
                    {
                        case tag_byte_array:
                            read_tag_array<int8_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            break;
                        case tag_int_array:
                            read_tag_array<int32_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            break;
                        case tag_long_array:
                            read_tag_array<int64_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    itr += array_len * (tag_properties[tag_type].size * -1);
                    primitives.emplace_back(tag_type, (uint64_t)array_ptr);
                }
            }
        }

        size = itr - itr_start;
        return itr;
    }

#pragma clang diagnostic pop
}