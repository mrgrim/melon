//
// Created by MrGrim on 8/14/2022.
//

#include <iostream>
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
    compound::compound(std::string_view name_in, int64_t max_size_in, std::pmr::memory_resource *upstream_pmr_rsrc)
            : parent(std::nullopt),
              top(this),
              depth(1),
              max_size(max_size_in),
              pmr_rsrc(new std::pmr::monotonic_buffer_resource(4096, upstream_pmr_rsrc)),
              compounds(init_container<compound *>()),
              lists(init_container<list *>()),
              primitives(init_container<primitive_tag *>())
    {
        void *ptr = pmr_rsrc->allocate(sizeof(std::pmr::string));
        name = new(ptr) std::pmr::string(name_in, pmr_rsrc);
    }

    compound::compound(compound &&in) noexcept
            : size(in.size),
              name(in.name),
              depth(in.depth),
              max_size(in.max_size),
              parent(in.parent),
              top(in.top),
              pmr_rsrc(in.pmr_rsrc),
              primitives(in.primitives),
              compounds(in.compounds),
              lists(in.lists)
    {
        std::cerr << "!!! Moving compound via ctor: " << name << "!" << std::endl;

        in.name   = nullptr;
        in.parent = std::nullopt;
        in.top    = nullptr;

        in.size     = 0;
        in.depth    = 1;
        in.max_size = -1;
        in.pmr_rsrc = std::pmr::get_default_resource();

        in.compounds  = nullptr;
        in.lists      = nullptr;
        in.primitives = nullptr;
    }

    compound &compound::operator=(compound &&in) noexcept
    {
        if (this != &in)
        {
            std::cerr << "!!! Moving compound via operator=: " << in.name << "!" << std::endl;

            name   = in.name;
            parent = in.parent;
            top    = in.top;

            size     = in.size;
            depth    = in.depth;
            max_size = in.max_size;
            pmr_rsrc = in.pmr_rsrc;

            primitives = in.primitives;
            compounds  = in.compounds;
            lists      = in.lists;

            in.name   = nullptr;
            in.parent = std::nullopt;
            in.top    = nullptr;

            in.size     = 0;
            in.depth    = 1;
            in.max_size = -1;
            in.pmr_rsrc = std::pmr::get_default_resource();

            in.compounds  = nullptr;
            in.lists      = nullptr;
            in.primitives = nullptr;
        }

        return *this;
    }

#if NBT_DEBUG == true
    compound::~compound()
    {
        if (name->empty())
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
    }
#endif

    compound::compound(std::unique_ptr<std::vector<std::byte>> raw, std::pmr::memory_resource *pmr_rsrc_in)
            : depth(1),
              max_size(-1),
              top(this),
              pmr_rsrc(pmr_rsrc_in),
              compounds(init_container<compound *>()),
              lists(init_container<list *>()),
              primitives(init_container<primitive_tag *>())
    {
        if (raw->size() < 5) throw std::runtime_error("NBT Compound Tag Too Small.");
        if (static_cast<tag_type_enum>(*raw->data()) != tag_compound) [[unlikely]] throw std::runtime_error("NBT Tag Type Not Compound.");

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

        read(raw->data(), raw->data() + raw->size());
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

    compound::compound(std::byte **itr_in, const std::byte *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in, bool no_header)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              name(name_in),
              pmr_rsrc(top->pmr_rsrc),
              compounds(init_container<compound *>()),
              lists(init_container<list *>()),
              primitives(init_container<primitive_tag *>())
    {
        if (depth > 512) throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, itr_end, no_header);
    }

    std::byte *compound::read(std::byte *itr, const std::byte *itr_end, bool skip_header)
    {
        static_assert(sizeof(tag_type_enum) == sizeof(std::byte));

        void *ptr;
        auto itr_start = itr;

        uint16_t name_len;
        std::memcpy(&name_len, itr + 1, sizeof(name_len));
        name_len = cvt_endian(name_len);

        if (name == nullptr)
        {
            ptr = pmr_rsrc->allocate(sizeof(std::pmr::string));
            name = new(ptr) std::pmr::string(reinterpret_cast<char *>(itr + 3), name_len & (static_cast<int16_t>(skip_header) - 1), pmr_rsrc);
        }

        itr += (name_len + sizeof(name_len) + sizeof(tag_type_enum)) & (static_cast<int16_t>(skip_header) - 1);

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (tag_type >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

        while ((itr_end - itr) && tag_type != tag_end)
        {
            std::memcpy(&name_len, itr, sizeof(name_len));
            name_len = cvt_endian(name_len);

            ptr            = pmr_rsrc->allocate(sizeof(std::pmr::string));
            auto *tag_name = new(ptr) std::pmr::string(reinterpret_cast<char *>(itr + sizeof(name_len)), name_len, pmr_rsrc);

            itr += sizeof(name_len) + name_len;

            if (tag_properties[tag_type].category == cat_container)
            {
                itr -= name_len + sizeof(name_len) + sizeof(tag_type_enum); // Roll back to start of tag for recursive processing

                if (tag_type == tag_list)
                {
#if NBT_DEBUG == true
                    std::cout << "Found List " << *tag_name << std::endl;
                    compound::lists_parsed++;
#endif
                    ptr = pmr_rsrc->allocate(sizeof(list));
                    (*lists)[*tag_name] = new(ptr) list(&itr, itr_end, this, tag_name);
                }
                else if (tag_type == tag_compound)
                {
#if NBT_DEBUG == true
                    std::cout << "Found Compound " << *tag_name << std::endl;
                    compound::compounds_parsed++;
#endif
                    ptr = pmr_rsrc->allocate(sizeof(compound));
                    (*compounds)[*tag_name] = new(ptr) compound(&itr, itr_end, this, tag_name);
                }
            }
            else if (tag_properties[tag_type].category == cat_primitive)
            {
                ptr = pmr_rsrc->allocate(sizeof(primitive_tag));
                (*primitives)[*tag_name] = new(ptr) primitive_tag(tag_type, read_tag_primitive(&itr, tag_type), tag_name);

#if NBT_DEBUG == true
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
                compound::primitives_parsed++;

                std::cout << "Found " << tag_printable_names[tag_type] << " primitive " << *(*primitives)[*tag_name]->name << ": ";
                switch (tag_type)
                {
                    case tag_byte:
                        std::cout << +(*primitives)[*tag_name]->value.tag_byte << std::endl;
                        break;
                    case tag_short:
                        std::cout << (*primitives)[*tag_name]->value.tag_short << std::endl;
                        break;
                    case tag_int:
                        std::cout << (*primitives)[*tag_name]->value.tag_int << std::endl;
                        break;
                    case tag_long:
                        std::cout << (*primitives)[*tag_name]->value.tag_long << std::endl;
                        break;
                    case tag_float:
                        std::cout << (*primitives)[*tag_name]->value.tag_float << std::endl;
                        break;
                    case tag_double:
                        std::cout << (*primitives)[*tag_name]->value.tag_double << std::endl;
                        break;
                }
#pragma clang diagnostic pop
#endif
            }
            else if (tag_properties[tag_type].category == cat_array)
            {
                ptr = pmr_rsrc->allocate(sizeof(primitive_tag));

                if (tag_type == tag_string)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    uint16_t str_len;
                    std::memcpy(&str_len, itr, sizeof(str_len));
                    str_len = cvt_endian(str_len);
                    itr += sizeof(str_len);

                    (*primitives)[*tag_name] = new(ptr) primitive_tag(tag_type, reinterpret_cast<uint64_t>(itr), tag_name, static_cast<uint32_t>(str_len));

                    itr += str_len;

#if NBT_DEBUG == true
                    std::cout << "Read string " << *(*primitives)[*tag_name]->name
                              << ": " << std::string_view((*primitives)[*tag_name]->value.tag_string, (*primitives)[*tag_name]->size) << std::endl;
                    compound::strings_parsed++;
#endif
                }
                else
                {

                    auto [array_ptr, array_len] = read_tag_array(&itr, tag_type, pmr_rsrc);

                    (*primitives)[*tag_name] = new(ptr) primitive_tag(tag_type, reinterpret_cast<uint64_t>(array_ptr), tag_name, static_cast<uint32_t>(array_len));

#if NBT_DEBUG == true
                    switch (tag_type)
                    {
                        case tag_byte_array:
                            std::cout << "Read byte array " << *(*primitives)[*tag_name]->name << ": ";
                            for (int index = 0; index < (*primitives)[*tag_name]->size; index++)
                                std::cout << +(*primitives)[*tag_name]->value.tag_byte_array[index] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
                            break;
                        case tag_int_array:
                            std::cout << "Read int array " << *(*primitives)[*tag_name]->name << ": ";
                            for (int index = 0; index < (*primitives)[*tag_name]->size; index++)
                                std::cout << +(*primitives)[*tag_name]->value.tag_int_array[index] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
                            break;
                        case tag_long_array:
                            std::cout << "Read long array " << *(*primitives)[*tag_name]->name << ": ";
                            for (int index = 0; index < (*primitives)[*tag_name]->size; index++)
                                std::cout << +(*primitives)[*tag_name]->value.tag_long_array[index] << " ";
                            std::cout << std::endl;
                            compound::arrays_parsed++;
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