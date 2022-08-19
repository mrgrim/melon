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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-use-after-move"
    compound::compound(compound &&in) noexcept
            : nbtcontainer(std::move(in)),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
    }
#pragma clang diagnostic pop

    compound &compound::operator=(compound &&in) noexcept
    {
        if (this != &in)
        {
            nbtcontainer::operator=(std::move(*this));
        }

        return *this;
    }

    compound::~compound()
    {
#if NBT_DEBUG == true
        if (name == nullptr)
            std::cout << "Deleting anonymous compound." << std::endl;
        else
            std::cout << "Deleting compound with name " << *name << std::endl;
#endif
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

    uint8_t *compound::read(std::vector<uint8_t> *raw_in, uint8_t *itr, bool skip_header)
    {
        if (depth() == 1 && (raw_in->max_size() - raw_in->size()) < 8)
        {
            // Later on we intentionally read up to 8 bytes past arbitrary locations so need the buffer to be 8 bytes
            // larger than the size of the data in it.

            std::cerr << "NBT read buffer requires resize. ("
                      << std::source_location::current().file_name() << ":"
                      << std::source_location::current().line() << "\n";
            raw_in->reserve(raw_in->size() + 8);
        }

        if (itr == nullptr) itr = raw_in->data();
        auto itr_start = itr;
        auto itr_end = raw_in->data() + raw_in->size();

        if (!skip_header)
        {
            if (raw_in->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");
            if (static_cast<tag_type_enum>(*itr++) != tag_compound) throw std::runtime_error("NBT Tag Type Not Compound.");

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
#if NBT_DEBUG == true
                std::cout << "Found end tag." << std::endl;
#endif
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
#if NBT_DEBUG == true
                    std::cout << "Found List " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
#endif
                    lists.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                  std::forward_as_tuple(raw_in, &itr, static_cast<nbtcontainer *>(this)));
                }
                else if (tag_type == tag_compound)
                {
#if NBT_DEBUG == true
                    std::cout << "Found Compound " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
#endif
                    compounds.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                      std::forward_as_tuple(raw_in, &itr, static_cast<nbtcontainer *>(this)));
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

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                switch (tag_type)
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
                }
#pragma clang diagnostic pop
#endif

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

#if NBT_DEBUG == true
                    std::cout << "Read string " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << str << std::endl;
#endif
                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, (uint64_t)str));
                    itr += str_len;
                }
                else
                {
                    const auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

#if NBT_DEBUG == true
                    std::cout << "Attempting array read of " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << " for " << array_len << " elements." << std::endl;
#endif
                    void *array_ptr;

                    switch (tag_type)
                    {
                        case tag_byte_array:
#if NBT_DEBUG == true
                            std::cout << "Byte Array Processing." << std::endl;
#endif
                            read_tag_array<int8_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read byte array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int8_t *)(array_ptr))[index] << " ";
                            std::cout << std::endl;
#endif
                            break;
                        case tag_int_array:
#if NBT_DEBUG == true
                            std::cout << "Int Array Processing." << std::endl;
#endif
                            read_tag_array<int32_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read int array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int32_t *)(array_ptr))[index] << " ";
                            std::cout << std::endl;
#endif
                            break;
                        case tag_long_array:
#if NBT_DEBUG == true
                            std::cout << "Long Array Processing." << std::endl;
#endif
                            read_tag_array<int64_t>(array_len, &array_ptr, reinterpret_cast<void *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read long array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int64_t *)(array_ptr))[index] << " ";
                            std::cout << std::endl;
#endif
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
#if NBT_DEBUG == true
        if (depth() == 1) std::cout << "Parsed " << size << " bytes of NBT data." << std::endl;
#endif

        if (depth() == 1)
            return nullptr;
        else
#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"
            return itr;
#pragma clang diagnostic pop
    }

#pragma clang diagnostic pop
}