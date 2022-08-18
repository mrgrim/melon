//
// Created by MrGrim on 8/14/2022.
//

#include <iostream>
#include <utility>
#include <source_location>
#include <cstring>
#include "nbt.h"

namespace melon::nbt
{

    compound::compound(compound &&in) noexcept
            : name(in.name), size(in.size),
              depth(in.depth), size_tracking(in.size_tracking),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        in.name = nullptr;

        in.size          = 0;
        in.depth         = 0;
        in.size_tracking = 0;
    }

    compound &compound::operator=(compound &&in) noexcept
    {
        if (this != &in)
        {
            delete name;
            // Don't delete raw. Its lifetime is outside the scope of the nbt library.

            name = in.name;

            size          = in.size;
            depth         = in.depth;
            size_tracking = in.size_tracking;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.name = nullptr;

            in.size          = 0;
            in.depth         = 0;
            in.size_tracking = 0;
        }

        return *this;
    }

    compound::~compound()
    {
        delete name;
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

    uint8_t *compound::read(std::vector<uint8_t> *raw, uint8_t *itr, uint32_t depth_in, bool skip_header)
    {
        depth = depth_in + 1;

        if (depth == 1 && (raw->max_size() - raw->size()) < 8)
        {
            // Later on we intentionally read up to 8 bytes past arbitrary locations so need the buffer to be 8 bytes
            // larger than the size of the data in it.

            std::cerr << "NBT read buffer requires resize. ("
                      << std::source_location::current().file_name() << ":"
                      << std::source_location::current().line() << "\n";
            raw->reserve(raw->size() + 8);
        }

        if (depth > 512) throw std::runtime_error("NBT Tags Nested Too Deeply (>512).");

        if (itr == nullptr) itr = raw->data();
        auto itr_start = itr;
        auto itr_end = raw->data() + raw->size();
        uint16_t name_len;

        if (!skip_header)
        {
            if (raw->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");
            if (static_cast<tag_type_enum>(*itr++) != tag_compound)
                throw std::runtime_error("NBT Tag Type Not Compound.");

            name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            delete name;
            name = new std::string(reinterpret_cast<char *>(itr), name_len);
            itr += name_len;
        }

        while (itr < itr_end)
        {
            auto tag_type = static_cast<tag_type_enum>(*itr++);
            if (tag_type >= tag_properties.size()) throw std::runtime_error("Invalid NBT Tag Type.");

            if (tag_type == tag_end)
            {
                break;
            }

            name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            auto tag_name = std::string(reinterpret_cast<char *>(itr), name_len);
            itr += name_len;

            if (tag_properties[tag_type].size == 127)
            {
                itr -= name_len + 3;

                if (tag_type == tag_list)
                {
                    list tag_list = list();
                    itr = tag_list.read(raw, itr, depth);

                    lists.insert({ std::move(tag_name), std::move(tag_list) });
                }
                else if (tag_type == tag_compound)
                {
                    compound tag_compound = compound();
                    itr = tag_compound.read(raw, itr, depth);

                    compounds.insert({ std::move(tag_name), std::move(tag_compound) });
                }
            }
            else if (tag_properties[tag_type].size >= 0)
            {
                // C++14 guarantees the start address is the same across union members making a memcpy safe to do
                // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
                primitive_tag p_tag = { tag_type };
                std::memcpy(reinterpret_cast<void *>(&(p_tag.value)), reinterpret_cast<void *>(itr), sizeof(p_tag.value));

                // Change endianness based on type size to keep it to these 3 cases. The following code should compile
                // away on big endian systems.
                switch (tag_properties[tag_type].size)
                {
                    case 2:
                        *(reinterpret_cast<uint16_t *>(&(p_tag.value))) = cvt_endian(*(reinterpret_cast<uint16_t *>(&(p_tag.value))));
                        break;
                    case 4:
                        *(reinterpret_cast<uint32_t *>(&(p_tag.value))) = cvt_endian(*(reinterpret_cast<uint32_t *>(&(p_tag.value))));
                        break;
                    case 8:
                        *(reinterpret_cast<uint64_t *>(&(p_tag.value))) = cvt_endian(*(reinterpret_cast<uint64_t *>(&(p_tag.value))));
                        break;
                }

                primitives.insert({ std::move(tag_name), std::move(p_tag) });
                itr += tag_properties[tag_type].size;
            }
            else if (tag_properties[tag_type].size < 0)
            {
                primitive_tag p_tag = { tag_type };

                if (tag_type == tag_string)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    auto str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                    itr += 2;

                    p_tag.value.tag_string = new std::string(reinterpret_cast<char *>(itr), str_len);
                    primitives.insert({ std::move(tag_name), std::move(p_tag) });
                    itr += str_len;
                }
                else
                {
                    auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

                    switch (tag_type)
                    {
                        case tag_byte_array:
                            p_tag.value.tag_byte_array = read_tag_array<int8_t>(array_len, reinterpret_cast<void *>(itr));
                            break;
                        case tag_int_array:
                            p_tag.value.tag_int_array = read_tag_array<int32_t>(array_len, reinterpret_cast<void *>(itr));
                            break;
                        case tag_long_array:
                            p_tag.value.tag_long_array = read_tag_array<int64_t>(array_len, reinterpret_cast<void *>(itr));
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    itr += array_len * (tag_properties[tag_type].size * -1);
                    primitives.insert({ std::move(tag_name), std::move(p_tag) });
                }
            }
        }

        size = itr - itr_start;
        if (depth == 1) std::cout << "Parsed " << size << " bytes of NBT data." << std::endl;

        if (depth == 1)
            return nullptr;
        else
#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"
            return itr;
#pragma clang diagnostic pop
    }

#pragma clang diagnostic pop
}