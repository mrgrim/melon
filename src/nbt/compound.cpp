//
// Created by MrGrim on 8/14/2022.
//

#include <cstring>
#include <memory>
#include <memory_resource>
#include "nbt.h"
#include "list.h"
#include "compound.h"

// For details on the file format go to: https://minecraft.fandom.com/wiki/NBT_format#Binary_format

namespace melon::nbt
{
    compound::compound(std::string_view name_in, int64_t max_size_in, std::unique_ptr<std::pmr::memory_resource> pmr_rsrc_in)
            : parent(std::nullopt),
              top(this),
              pmr_rsrc(pmr_rsrc_in == nullptr ? new std::pmr::monotonic_buffer_resource(64 * 1024) : pmr_rsrc_in.release()),
              tags(init_container()),
              depth(1),
              max_size(max_size_in)
    {
        void *ptr = pmr_rsrc->allocate(sizeof(std::pmr::string));
        name = new(ptr) std::pmr::string(name_in, pmr_rsrc);
    }

    compound::compound(std::unique_ptr<char[]> raw, size_t raw_size, std::unique_ptr<std::pmr::memory_resource> pmr_rsrc_in)
            : top(this),
              pmr_rsrc(pmr_rsrc_in == nullptr ? new std::pmr::monotonic_buffer_resource(64 * 1024) : pmr_rsrc_in.release()),
              tags(init_container()),
              depth(1),
              max_size(-1)
    {
        if (raw_size < 5) throw std::runtime_error("NBT Compound Tag Too Small.");

        auto itr = raw.get();
        if (static_cast<tag_type_enum>(*itr++) != tag_compound) [[unlikely]] throw std::runtime_error("NBT tag type not compound.");

        uint16_t name_len;
        std::memcpy(&name_len, itr, sizeof(name_len));
        name_len = cvt_endian(name_len);

        auto ptr = pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string));
        name = new(ptr) std::pmr::string(itr + 2, name_len, pmr_rsrc);

        itr += name_len + sizeof(name_len);
        read(itr, raw.get() + raw_size);
    }

    compound::compound(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in)
            : name(name_in),
              parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              tags(init_container())
    {
        if (depth > 512) throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, itr_end);
    }

    compound::~compound()
    {
        if (this == top) delete pmr_rsrc;
    }

    char *compound::read(char *itr, const char *const itr_end)
    {
        static_assert(sizeof(tag_type_enum) == sizeof(std::byte));

        void     *ptr;
        auto     itr_start = itr;
        uint16_t name_len;

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (static_cast<uint8_t>(tag_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

        while ((itr_end - itr) >= 2 && tag_type != tag_end)
        {
            std::memcpy(&name_len, itr, sizeof(name_len));
            name_len = cvt_endian(name_len);

            // The constructor ensures at least 8 bytes past the end of the compound is allocated.
            // This covers list, compound, and basic primitive.
            if ((itr + sizeof(name_len) + name_len + 8) >= itr_end)
                [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

            ptr = pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string));
            auto *tag_name = new(ptr) std::pmr::string(itr + sizeof(name_len), name_len, pmr_rsrc);

            itr += sizeof(name_len) + name_len;

            if (tag_properties[tag_type].category & (cat_compound | cat_list))
            {
                if (tag_type == tag_list)
                {
                    auto list_type = static_cast<tag_type_enum>(*itr++);
                    if (static_cast<uint8_t>(list_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

                    ptr = pmr_rsrc->allocate(sizeof(list), alignof(list));
                    (*tags)[*tag_name] = new(ptr) list(&itr, itr_end, this, tag_name, list_type);
                }
                else if (tag_type == tag_compound)
                {
                    ptr = pmr_rsrc->allocate(sizeof(compound), alignof(compound));
                    (*tags)[*tag_name] = new(ptr) compound(&itr, itr_end, this, tag_name);
                }
            }
            else if (tag_properties[tag_type].category == cat_primitive)
            {
                ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                (*tags)[*tag_name] = new(ptr) primitive_tag(tag_type, read_tag_primitive(&itr, tag_type), tag_name);
            }
            else if (tag_properties[tag_type].category & (cat_array | cat_string))
            {
                ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));

                if (tag_type == tag_string)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    uint16_t str_len;

                    std::memcpy(&str_len, itr, sizeof(str_len));
                    str_len = cvt_endian(str_len);

                    if ((itr + sizeof(str_len) + str_len + 8) >= itr_end)
                        [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                    void *str_ptr = pmr_rsrc->allocate(str_len, alignof(char *));
                    std::memcpy(str_ptr, itr + sizeof(str_len), str_len);

                    (*tags)[*tag_name] = new(ptr) primitive_tag(tag_type, std::bit_cast<uint64_t>(str_ptr), tag_name, static_cast<uint32_t>(str_len));

                    itr += sizeof(str_len) + str_len;
                }
                else
                {
                    // Bounds check is done in call
                    auto [array_ptr, array_len] = read_tag_array(&itr, itr_end, tag_type, pmr_rsrc);

                    (*tags)[*tag_name] = new(ptr) primitive_tag(tag_type, std::bit_cast<uint64_t>(array_ptr), tag_name, static_cast<uint32_t>(array_len));
                }
            }

            tag_type = static_cast<tag_type_enum>(*itr++);
            if (static_cast<uint8_t>(tag_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");
        }

        if (tag_type != tag_end) [[unlikely]] throw std::runtime_error("NBT compound parsing ended before reaching END tag.");

        size = itr - itr_start;
        return itr;
    }

    void compound::to_snbt(std::string &out)
    {
        if (name != nullptr && !name->empty())
        {
            snbt::escape_string(*name, out, false);
            out.push_back(':');
        }

        out.push_back('{');
        uint32_t processed_count = 0;

        for (const auto &[_, tag]: *tags)
        {
            std::visit([&processed_count, &out](auto &&tag_in) {
                if (tag_in->name == nullptr || tag_in->name->empty()) [[unlikely]] throw std::runtime_error("Unexpected anonymous tag in NBT compound.");

                tag_in->to_snbt(out);
                out.push_back(',');

                processed_count++;
            }, tag);
        }

        if (processed_count)
            out.back() = '}';
        else
            out.push_back('}');
    }

    std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>> *compound::init_container()
    {
        auto ptr = pmr_rsrc->allocate(sizeof(std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>>),
                                      alignof(std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>>));
        return new(ptr) std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>>(pmr_rsrc);
    }

    primitive_tag *compound::get_primitive(std::string_view tag_name, tag_type_enum tag_type)
    {
        auto          itr = tags->find(tag_name);
        primitive_tag *tag_ptr;

        if (itr == tags->end())
        {
            auto *str_ptr = static_cast<std::pmr::string *>(pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string)));
            tag_ptr = static_cast<primitive_tag *>(pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag)));

            str_ptr = new(str_ptr) std::pmr::string(tag_name, pmr_rsrc);
            tag_ptr = new(tag_ptr) primitive_tag(tag_type, 0, str_ptr);

            (*tags)[*str_ptr] = tag_ptr;
        }
        else
        {
            if (!(std::holds_alternative<primitive_tag *>(itr->second) &&
                  (tag_ptr = std::get<primitive_tag *>(itr->second))->tag_type == tag_type))
                [[unlikely]] throw std::runtime_error("Attempted to write wrong type to NBT tag.");
        }

        return tag_ptr;
    }
}