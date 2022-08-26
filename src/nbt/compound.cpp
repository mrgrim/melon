//
// Created by MrGrim on 8/14/2022.
//

#include <iostream>
#include <utility>
#include <cstring>
#include <memory>
#include <memory_resource>
#include "nbt.h"
#include "list.h"
#include "compound.h"

#if DEBUG == true
#include <iostream>
#include <source_location>
#endif

// For details on the file format go to: https://minecraft.fandom.com/wiki/NBT_format#Binary_format

namespace melon::nbt
{

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    compound::compound(std::optional<std::variant<compound *, list *>> parent_in, int64_t max_size_in,
                       std::pmr::memory_resource *pmr_rsrc_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in.value_or(this))),
              pmr_rsrc(pmr_rsrc_in),
              primitives(pmr_rsrc),
              compounds(pmr_rsrc),
              lists(pmr_rsrc)
    {
        if (parent)
        {
            std::visit([this](auto &&tag) {
                depth    = tag->depth + 1;
                max_size = tag->max_size;
            }, parent_in.value());
        }
        else
        {
            depth    = 1;
            max_size = max_size_in;
        }
    }

    compound::compound(std::unique_ptr<std::vector<std::byte>> raw_in, std::pmr::memory_resource *pmr_rsrc_in)
            : depth(1),
              max_size(-1),
              top(this),
              raw(std::move(raw_in)),
              pmr_rsrc(pmr_rsrc_in),
              primitives(pmr_rsrc),
              compounds(pmr_rsrc),
              lists(pmr_rsrc)
    {
        if (raw->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");

        if ((raw->capacity() - raw->size()) < 8)
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

        read(raw->data());
    }

#pragma clang diagnostic pop

    compound::compound(compound &&in) noexcept
            : size(in.size),
              name(in.name),
              depth(in.depth),
              max_size(in.max_size),
              parent(in.parent),
              top(in.top),
              raw(std::move(in.raw)),
              name_backing(in.name_backing),
              pmr_rsrc(in.pmr_rsrc),
              primitives(std::move(in.primitives)),
              compounds(std::move(in.compounds)),
              lists(std::move(in.lists))
    {
        std::cerr << "!!! Moving compound via ctor: " << name << "!" << std::endl;

        in.name_backing = nullptr;
        in.parent       = std::nullopt;
        in.top          = nullptr;

        in.name     = std::string_view();
        in.size     = 0;
        in.depth    = 1;
        in.max_size = -1;
        in.pmr_rsrc = std::pmr::get_default_resource();
    }

    compound &compound::operator=(compound &&in) noexcept
    {
        if (this != &in)
        {
            std::cerr << "!!! Moving compound via operator=: " << in.name << "!" << std::endl;

            delete (name_backing);

            name   = in.name;
            parent = in.parent;
            top    = in.top;

            size         = in.size;
            depth        = in.depth;
            max_size     = in.max_size;
            name_backing = in.name_backing;
            pmr_rsrc     = in.pmr_rsrc;

            raw        = std::move(in.raw);
            primitives = std::move(in.primitives);
            compounds  = std::move(in.compounds);
            lists      = std::move(in.lists);

            in.name   = std::string_view();
            in.parent = std::nullopt;
            in.top    = nullptr;

            in.size         = 0;
            in.depth        = 1;
            in.max_size     = -1;
            in.name_backing = nullptr;
            in.pmr_rsrc     = std::pmr::get_default_resource();
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

        if (top == this)
        {
            std::cout << "Top level compound destructor called.\n";
            std::cout << "Compounds parsed: " << compounds_parsed << "\n";
            std::cout << "Lists parsed: " << lists_parsed << "\n";
            std::cout << "Strings parsed: " << strings_parsed << "\n";
            std::cout << "Arrays parsed: " << arrays_parsed << "\n";
            std::cout << "primitives parsed: " << primitives_parsed << std::endl;
        }
#endif
        delete name_backing;
    }

    compound::compound(std::byte **itr_in, std::variant<compound *, list *> parent_in, bool skip_header)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              primitives(pmr_rsrc),
              compounds(pmr_rsrc),
              lists(pmr_rsrc)
    {
        if (depth > 512) throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, skip_header);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "LocalValueEscapesScope"

    std::byte *compound::read(std::byte *itr, bool skip_header)
    {
        static_assert(sizeof(tag_type_enum) == sizeof(std::byte));

        auto itr_start = itr;
        auto itr_end   = top->raw->data() + top->raw->size();

        if (static_cast<tag_type_enum>(*itr) != tag_compound) [[unlikely]] throw std::runtime_error("NBT Tag Type Not Compound.");

        uint16_t name_len;
        std::memcpy(&name_len, itr + 1, sizeof(name_len));
        name_len = cvt_endian(name_len);

        name = std::string_view(reinterpret_cast<char *>(itr + 3), name_len & (static_cast<int16_t>(skip_header) - 1));

        itr += (name_len + sizeof(name_len) + sizeof(tag_type_enum)) & (static_cast<int16_t>(skip_header) - 1);

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

        while ((itr_end - itr) && tag_type != tag_end)
        {
            std::memcpy(&name_len, itr, sizeof(name_len));
            name_len = cvt_endian(name_len);

            const auto name_ptr = itr + sizeof(name_len);
            itr += sizeof(name_len) + name_len;

            if (tag_properties[tag_type].category == cat_complex)
            {
                itr -= name_len + sizeof(name_len) + sizeof(tag_type_enum); // Roll back to start of tag for recursive processing

                if (tag_type == tag_list)
                {
#if NBT_DEBUG == true
                    std::cout << "Found List " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
                    compound::lists_parsed++;
#endif
                    lists.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                  std::forward_as_tuple(&itr, this));
                }
                else if (tag_type == tag_compound)
                {
#if NBT_DEBUG == true
                    std::cout << "Found Compound " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << std::endl;
                    compound::compounds_parsed++;
#endif
                    compounds.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                      std::forward_as_tuple(&itr, this));
                }
            }
            else if (tag_properties[tag_type].category == cat_primitive)
            {
                primitives.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                   std::forward_as_tuple(tag_type, read_tag_primitive(&itr, tag_type)));

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                compound::primitives_parsed++;

                std::cout << "Found " << tag_printable_names[tag_type] << " primitive " << std::string_view(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                switch (tag_type)
                {
                    case tag_byte:
                        std::cout << +primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_byte << std::endl;
                        break;
                    case tag_short:
                        std::cout << primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_short << std::endl;
                        break;
                    case tag_int:
                        std::cout << primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_int << std::endl;
                        break;
                    case tag_long:
                        std::cout << primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_long << std::endl;
                        break;
                    case tag_float:
                        std::cout << primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_float << std::endl;
                        break;
                    case tag_double:
                        std::cout << primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_double << std::endl;
                        break;
                }
#pragma clang diagnostic pop
#endif
            }
            else if (tag_properties[tag_type].category == cat_array)
            {
                if (tag_type == tag_string)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    uint16_t str_len;
                    std::memcpy(&str_len, itr, sizeof(str_len));
                    str_len = cvt_endian(str_len);
                    itr += sizeof(str_len);

#if NBT_DEBUG == true
                    std::cout << "Read string " << std::string_view(reinterpret_cast<char *>(name_ptr), name_len)
                              << ": " << std::string_view(reinterpret_cast<char *>(itr), str_len) << std::endl;
                    compound::strings_parsed++;
#endif
                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, reinterpret_cast<uint64_t>(itr), static_cast<uint32_t>(str_len)));

                    itr += str_len;

                    // These strings get null terminated by the zeroing out of the tag_type byte earlier or the final zero at the end of processing.
                }
                else
                {
                    auto [array_ptr, array_len] = read_tag_array(&itr, tag_type, pmr_rsrc);

                    primitives.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(reinterpret_cast<char *>(name_ptr), name_len),
                                       std::forward_as_tuple(tag_type, reinterpret_cast<uint64_t>(array_ptr), static_cast<uint32_t>(array_len)));

#if NBT_DEBUG == true
                    switch (tag_type)
                    {
                        case tag_byte_array:
                            std::cout << "Read byte array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++)
                                std::cout << +primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_byte_array[index] << " ";
                            std::cout << std::endl;
                            break;
                        case tag_int_array:
                            std::cout << "Read int array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++)
                                std::cout << +primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_int_array[index] << " ";
                            std::cout << std::endl;
                            break;
                        case tag_long_array:
                            std::cout << "Read long array " << std::string(reinterpret_cast<char *>(name_ptr), name_len) << ": ";
                            for (int index = 0; index < array_len; index++)
                                std::cout << +primitives[std::string_view(reinterpret_cast<char *>(name_ptr), name_len)].value.tag_long_array[index] << " ";
                            std::cout << std::endl;
                            break;
                        default:
                            // Shouldn't be possible
                            [[unlikely]] throw std::runtime_error("Unexpected NBT Array Type.");
                    }
#endif
                }
            }

            tag_type = static_cast<tag_type_enum>(*itr++);
            if (tag_type >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");
        }

        size = itr - itr_start;
#if NBT_DEBUG == true
        if (depth == 1) std::cout << "Parsed " << size << " bytes of NBT data." << std::endl;

        if (tag_type == tag_end)
        {
            std::cout << "Found end tag";
        }

        if (!(itr_end - itr))
        {
            std::cout << " and found end of iterator";
        }

        std::cout << "." << std::endl;
#endif

        return itr;
    }

#pragma clang diagnostic pop
}