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
        /*if (name == nullptr)
            std::cout << "Deleting anonymous compound." << std::endl;
        else
            std::cout << "Deleting compound with name " << *name << std::endl;*/

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

        if (!skip_header)
        {
            if (raw->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");
            if (static_cast<tag_type_enum>(*itr++) != tag_compound)
                throw std::runtime_error("NBT Tag Type Not Compound.");

            const auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
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
                //std::cout << "Found end tag." << std::endl;
                break;
            }

            const auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;
            const auto name_ptr = itr;
            itr += name_len;

            if (tag_properties[tag_type].size == 127)
            {
                itr -= 3 + name_len;

                if (tag_type == tag_list)
                {
                    //std::cout << "Found List " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
                    lists.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                  std::forward_as_tuple(depth, raw, &itr));
                }
                else if (tag_type == tag_compound)
                {
                    //std::cout << "Found Compound " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
                    compounds.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                      std::forward_as_tuple(depth, raw, &itr));
                }
            }
            else if (tag_properties[tag_type].size >= 0)
            {
                uint64_t prim_value;
                // C++14 guarantees the start address is the same across union members making a memcpy safe to do
                // Always copy 8 bytes because it'll allow the memcpy to be inlined easily.
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

                /*switch (tag_type)
                {
                    case tag_byte:
                        std::cout << "Found byte primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << +(*((uint8_t *)(&prim_value) + 7)) << std::endl;
                        break;
                    case tag_short:
                        std::cout << "Found short primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((uint16_t *)(&prim_value) + 3) << std::endl;
                        break;
                    case tag_int:
                        std::cout << "Found int primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((uint32_t *)(&prim_value) + 1) << std::endl;
                        break;
                    case tag_long:
                        std::cout << "Found long primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((uint64_t *)(&prim_value)) << std::endl;
                        break;
                    case tag_float:
                        std::cout << "Found float primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((float *)(&prim_value) + 1) << std::endl;
                        break;
                    case tag_double:
                        std::cout << "Found double primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((double *)(&prim_value)) << std::endl;
                        break;
                }*/

                primitives.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                   std::forward_as_tuple(tag_type, *(reinterpret_cast<uint64_t *>(&prim_value))));
                itr += tag_properties[tag_type].size;
            }
            else if (tag_properties[tag_type].size < 0)
            {
                if (tag_type == tag_string)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    const auto str_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
                    itr += 2;

                    auto str = (char *)malloc(str_len + 1);
                    std::memcpy((void *)str, (void *)itr, str_len);
                    str[str_len] = 0;

                    //std::cout << "Read string " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << str << std::endl;
                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, (uint64_t)str));
                    itr += str_len;
                }
                else
                {
                    const auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

                    //std::cout << "Attempting array read of " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << " for " << array_len << " elements." << std::endl;
                    void *array_ptr;

                    switch (tag_type)
                    {
                        case tag_byte_array:
                            //std::cout << "Byte Array Processing." << std::endl;
                            read_tag_array<int8_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            //std::cout << "Read byte array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            //for (int index = 0; index < array_len; index++) std::cout << +((int8_t *)(array_ptr))[index] << " ";
                            //std::cout << std::endl;
                            break;
                        case tag_int_array:
                            //std::cout << "Int Array Processing." << std::endl;
                            read_tag_array<int32_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            //std::cout << "Read int array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            //for (int index = 0; index < array_len; index++) std::cout << +((int32_t *)(array_ptr))[index] << " ";
                            //std::cout << std::endl;
                            break;
                        case tag_long_array:
                            //std::cout << "Long Array Processing." << std::endl;
                            read_tag_array<int64_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
                            //std::cout << "Read long array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            //for (int index = 0; index < array_len; index++) std::cout << +((int64_t *)(array_ptr))[index] << " ";
                            //std::cout << std::endl;
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    itr += array_len * (tag_properties[tag_type].size * -1);
                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, (uint64_t)array_ptr));
                }
            }
        }

        size = itr - itr_start;
        //if (depth == 1) std::cout << "Parsed " << size << " bytes of NBT data." << std::endl;

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