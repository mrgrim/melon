//
// Created by MrGrim on 8/14/2022.
//

#include <optional>
#include <stdexcept>
#include <cstring>
#include <memory_resource>
#include "nbt.h"
#include "compound.h"
#include "list.h"

namespace melon::nbt
{
    list::list(std::variant<compound *, list *> parent_in, std::string_view name_in, tag_type_enum tag_type_in)
            : type(tag_type_in),
              parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              tags(init_container())
    {
        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        void *ptr = pmr_rsrc->allocate(sizeof(std::pmr::string), alignof(std::pmr::string));
        name = new(ptr) std::pmr::string(name_in, pmr_rsrc);
    }

    list::list(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, std::pmr::string *name_in, tag_type_enum tag_type_in)
            : name(name_in),
              type(tag_type_in),
              parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              tags(init_container())
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, itr_end);
    }

    char *list::read(char *itr, const char *const itr_end)
    {
        static_assert(sizeof(count) == sizeof(int32_t));
        static_assert(sizeof(tag_type_enum) == sizeof(char));

        void *ptr;
        auto itr_start = itr;

        std::memcpy(static_cast<void *>(&count), static_cast<const void *>(itr), sizeof(count));
        count = cvt_endian(count);
        tags->reserve(count);
        itr += sizeof(count);

        if (tag_properties[type].category & (cat_compound | cat_list))
        {
            if (type == tag_list)
            {
                for (int32_t index = 0; index < count; index++)
                {
                    if ((itr + sizeof(tag_type_enum) + 8) >= itr_end)
                        [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                    auto list_type = static_cast<tag_type_enum>(*itr++);
                    if (static_cast<uint8_t>(list_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

                    ptr = pmr_rsrc->allocate(sizeof(list), alignof(list));
                    tags->push_back(new(ptr) list(&itr, itr_end, this, nullptr, list_type));
                }
            }
            else if (type == tag_compound)
            {
                for (int32_t index = 0; index < count; index++)
                {
                    if ((itr + sizeof(tag_type_enum) + 8) >= itr_end)
                        [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                    ptr = pmr_rsrc->allocate(sizeof(compound), alignof(compound));
                    tags->push_back(new(ptr) compound(&itr, itr_end, this, nullptr));
                }
            }
        }
        else if (tag_properties[type].category == cat_primitive)
        {
            for (int32_t index = 0; index < count; index++)
            {
                if ((itr + tag_properties[type].size + 8) >= itr_end)
                    [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                tags->push_back(new(ptr) primitive_tag(type, read_tag_primitive(&itr, type)));
            }
        }
        else if (tag_properties[type].category & (cat_array | cat_string))
        {
            if (type == tag_string)
            {
                for (int32_t index = 0; index < count; index++)
                {
                    // Reminder: NBT strings are "Modified UTF-8" and not null terminated.
                    // https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
                    uint16_t str_len;

                    std::memcpy(&str_len, itr, sizeof(str_len));
                    str_len = cvt_endian(str_len);

                    if ((itr + sizeof(str_len) + str_len + 8) >= itr_end)
                        [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                    void *str_ptr = pmr_rsrc->allocate(str_len, alignof(char *));
                    std::memcpy(str_ptr, static_cast<const void *>(itr + sizeof(str_len)), str_len);

                    ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                    tags->push_back(new(ptr) primitive_tag(type, std::bit_cast<uint64_t>(str_ptr), nullptr, static_cast<uint32_t>(str_len)));

                    itr += sizeof(str_len) + str_len;
                }
            }
            else
            {
                for (int32_t index = 0; index < count; index++)
                {
                    // Bounds checking done in call
                    auto [array_ptr, array_len] = read_tag_array(&itr, itr_end, type, pmr_rsrc);

                    ptr = pmr_rsrc->allocate(sizeof(primitive_tag), alignof(primitive_tag));
                    tags->push_back(new(ptr) primitive_tag(type, std::bit_cast<uint64_t>(array_ptr), nullptr, static_cast<uint32_t>(array_len)));
                }
            }
        }

        size = itr - itr_start;
        return itr;
    }

    std::pmr::vector<void *> *list::init_container()
    {
        auto ptr = pmr_rsrc->allocate(sizeof(std::pmr::vector<void *>), alignof(std::pmr::vector<void *>));
        return new(ptr) std::pmr::vector<void *>(pmr_rsrc);
    }

    void list::to_snbt(std::string &out)
    {
        if (name != nullptr && !name->empty())
        {
            snbt::escape_string(*name, out, false);
            out.push_back(':');
        }

        out.push_back('[');

        if (count > 0 && type != tag_end)
        {
            auto process_entries = [this, &out]<typename T>(void **list_ptr) {
                for (int32_t idx = 0; idx < count; idx++)
                {
                    auto entry = static_cast<T *>(list_ptr[idx]);
                    if (entry->name != nullptr && !entry->name->empty()) [[unlikely]] throw std::runtime_error("Unexpected named tag in NBT list.");

                    entry->to_snbt(out);
                    out.push_back(',');
                }
            };

            if (type == tag_list)
                process_entries.template operator()<list>(tags->data());
            else if (type == tag_compound)
                process_entries.template operator()<compound>(tags->data());
            else
                process_entries.template operator()<primitive_tag>(tags->data());

            out.back() = ']';
        }
        else
            out.push_back(']');
    }
}