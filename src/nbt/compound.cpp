//
// Created by MrGrim on 8/14/2022.
//

#include <iostream>
#include <utility>
#include <cstring>
#include "nbt.h"
#include "list.h"
#include "compound.h"

#if DEBUG == true
#include <iostream>
#include <source_location>
#endif

namespace melon::nbt
{

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    compound::compound(std::optional<std::variant<compound *, list *>> parent_in, int64_t max_size_in)
    {
        if (parent_in)
        {
            parent = parent_in.value();
            if (std::holds_alternative<list *>(parent))
            {
                depth    = std::get<list *>(parent)->depth + 1;
                max_size = std::get<list *>(parent)->max_size;
                top      = std::get<list *>(parent)->top;
            }
            else
            {
                depth    = std::get<list *>(parent)->depth + 1;
                max_size = std::get<list *>(parent)->max_size;
                top      = std::get<compound *>(parent)->top;
            }
        }
        else
        {
            top      = this;
            depth    = 1;
            max_size = max_size_in;
        }
    }

    compound::compound(std::unique_ptr<std::vector<uint8_t>> raw_in, int64_t max_size_in)
            : depth(1), max_size(max_size_in), top(this), raw(std::move(raw_in))
    {
        if (raw->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");

        if ((raw->max_size() - raw->size()) < 8)
        {
            // Later on we intentionally read up to 8 bytes past arbitrary locations so need the buffer to be 8 bytes
            // larger than the size of the data in it.
#if DEBUG == true
            std::cerr << "NBT read buffer requires resize. ("
                      << std::source_location::current().file_name() << ":"
                      << std::source_location::current().line() << "\n";
#endif
            raw->reserve(raw->size() + 8);
        }

        read(raw->data(), false);
    }

#pragma clang diagnostic pop

    compound::compound(compound &&in) noexcept
            : size(in.size), name(in.name), depth(in.depth),
              size_tracking(in.size_tracking), max_size(in.max_size),
              readonly(in.readonly), parent(in.parent), name_backing(in.name_backing),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        in.name_backing = nullptr;
        in.parent       = (compound *)nullptr;

        in.name          = std::string_view();
        in.size          = 0;
        in.depth         = 1;
        in.size_tracking = 0;
        in.max_size      = -1;
        in.readonly      = false;
    }

    compound &compound::operator=(compound &&in) noexcept
    {
        if (this != &in)
        {
            delete (name_backing);

            name   = in.name;
            parent = in.parent;

            size          = in.size;
            depth         = in.depth;
            size_tracking = in.size_tracking;
            max_size      = in.max_size;
            readonly      = in.readonly;
            name_backing  = in.name_backing;

            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.name   = std::string_view();
            in.parent = (compound *)nullptr;

            in.size          = 0;
            in.depth         = 1;
            in.size_tracking = 0;
            in.max_size      = -1;
            in.readonly      = false;
            in.name_backing  = nullptr;
        }

        return *this;
    }

    compound::~compound()
    {
#if NBT_DEBUG == true
        if (name.empty())
            std::cout << "Deleting anonymous compound." << std::endl;
        else
            std::cout << "Deleting compound with name " << name << std::endl;
#endif
        delete name_backing;
    }

    compound::compound(uint8_t **itr_in, compound *parent_in, bool skip_header)
            : depth(parent_in->depth + 1),
              max_size(parent_in->max_size),
              size_tracking(parent_in->size_tracking),
              parent(parent_in->parent),
              top(parent_in->top)
    {
        if (depth > 512) throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }

    compound::compound(uint8_t **itr_in, list *parent_in, bool skip_header)
            : depth(parent_in->depth + 1),
              max_size(parent_in->max_size),
              size_tracking(parent_in->size_tracking),
              parent(parent_in->parent),
              top(parent_in->top)
    {
        if (depth > 512) throw std::runtime_error("NBT Depth exceeds 512.");

        *itr_in = read(*itr_in, skip_header);
    }


#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    uint8_t *compound::read(uint8_t *itr, bool skip_header)
    {
        auto itr_start = itr;
        auto itr_end   = top->raw->data() + top->raw->size();

        if (!skip_header)
        {
            if (static_cast<tag_type_enum>(*itr++) != tag_compound) throw std::runtime_error("NBT Tag Type Not Compound.");

            const auto name_len = cvt_endian(*(reinterpret_cast<uint16_t *>(itr)));
            itr += 2;

            name = std::string_view(reinterpret_cast<char *>(itr), name_len);

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
                                  std::forward_as_tuple(&itr, this));
                }
                else if (tag_type == tag_compound)
                {
#if NBT_DEBUG == true
                    std::cout << "Found Compound " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
#endif
                    compounds.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                      std::forward_as_tuple(&itr, this));
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
                        std::cout << "Found byte primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << +(*((int8_t *)(&prim_value)))
                                  << std::endl;
                        break;
                    case tag_short:
                        std::cout << "Found short primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((int16_t *)(&prim_value)) << std::endl;
                        break;
                    case tag_int:
                        std::cout << "Found int primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((int32_t *)(&prim_value)) << std::endl;
                        break;
                    case tag_long:
                        std::cout << "Found long primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((int64_t *)(&prim_value)) << std::endl;
                        break;
                    case tag_float:
                        std::cout << "Found float primitive " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": " << *((float *)(&prim_value)) << std::endl;
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

#if NBT_DEBUG == true
                    std::cout << "Read string " << std::string_view(reinterpret_cast<char *>(name_ptr), name_len)
                              << ": " << std::string_view(reinterpret_cast<char *>(itr), str_len) << std::endl;
#endif
                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, (uint64_t)itr, static_cast<uint32_t>(str_len)));

                    itr += str_len;

                    // These strings get null terminated by the zeroing out of the tag_type byte earlier or the final zero at the end of processing.
                }
                else
                {
                    // This approach of swapping the arrays in place avoids allocating memory, but could be expensive or even illegal for architectures that
                    // require aligned access. I need a way to detect this and compensate at runtime. We have 6 bytes of wiggle room (5 at the head, 1 at the tail) to
                    // move the array. Even that would technically be UB since to check the offset would require performing a bitwise operation on the pointer.
                    const auto array_len = cvt_endian(*(reinterpret_cast<int32_t *>(itr)));
                    itr += 4;

#if NBT_DEBUG == true
                    std::cout << "Attempting array read of " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << " for " << array_len << " elements." << std::endl;
#endif
                    switch (tag_type)
                    {
                        case tag_byte_array:
#if NBT_DEBUG == true
                            std::cout << "Byte Array Processing." << std::endl;
#endif
                            cvt_endian_array<int8_t>(array_len, reinterpret_cast<int8_t *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read byte array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int8_t *)(itr))[index] << " ";
                            std::cout << std::endl;
#endif
                            break;
                        case tag_int_array:
#if NBT_DEBUG == true
                            std::cout << "Int Array Processing." << std::endl;
#endif
                            cvt_endian_array<int32_t>(array_len, reinterpret_cast<int32_t *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read int array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int32_t *)(itr))[index] << " ";
                            std::cout << std::endl;
#endif
                            break;
                        case tag_long_array:
#if NBT_DEBUG == true
                            std::cout << "Long Array Processing." << std::endl;
#endif
                            cvt_endian_array<int64_t>(array_len, reinterpret_cast<int64_t *>(itr));
#if NBT_DEBUG == true
                            std::cout << "Read long array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++) std::cout << +((int64_t *)(itr))[index] << " ";
                            std::cout << std::endl;
#endif
                            break;
                        default:
                            // Shouldn't be possible
                            throw std::runtime_error("Unexpected NBT Array Type.");
                    }

                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, (uint64_t)itr, static_cast<uint32_t>(array_len)));

                    itr += array_len * (tag_properties[tag_type].size * -1);
                }
            }
        }

        size = itr - itr_start;
#if NBT_DEBUG == true
        if (depth == 1) std::cout << "Parsed " << size << " bytes of NBT data." << std::endl;
#endif

        if (depth == 1)
            return nullptr;
        else
            return itr;
    }

#pragma clang diagnostic pop
}