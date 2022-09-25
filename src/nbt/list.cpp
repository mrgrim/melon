//
// Created by MrGrim on 8/14/2022.
//

#include <optional>
#include <stdexcept>
#include <memory_resource>
#include "nbt.h"
#include "compound.h"
#include "list.h"

namespace melon::nbt
{
    list::list(std::variant<compound *, list *> parent_in, std::string_view name_in, tag_type_enum tag_type_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              name(mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, name_in)),
              type(tag_type_in),
              tags(tag_list_t(pmr_rsrc))
    {
        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);
    }

    list::list(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, mem::pmr::unique_ptr<std::pmr::string> name_in, tag_type_enum tag_type_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              name(std::move(name_in)),
              type(tag_type_in),
              tags(tag_list_t(pmr_rsrc))
    {
        if (depth > 512)
            throw std::runtime_error("NBT Depth exceeds 512.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        *itr_in = read(*itr_in, itr_end);
    }

    list::~list()
    {
        for (const auto &tag: tags)
        {
            if (tag_properties[type].category & (cat_primitive | cat_string | cat_array))
            {
                auto prim_tag = static_cast<primitive_tag *>(tag);

                if (tag_properties[type].category & (cat_array | cat_string))
                    pmr_rsrc->deallocate(prim_tag->value.generic_ptr, prim_tag->size() * tag_properties[type].size + padding_size, tag_properties[type].size);

                std::destroy_at(prim_tag);
                pmr_rsrc->deallocate(tag, sizeof(primitive_tag), alignof(primitive_tag));
            }
            else if (type == tag_compound)
            {
                std::destroy_at(static_cast<compound *>(tag));
                pmr_rsrc->deallocate(tag, sizeof(compound), alignof(compound));
            }
            else if (type == tag_list)
            {
                std::destroy_at(static_cast<list *>(tag));
                pmr_rsrc->deallocate(tag, sizeof(list), alignof(list));
            }
        }

        std::visit([this](auto &&parent_in) {
            parent_in->adjust_size(this->size * -1);
            parent_in->remove_container(this);
        }, parent);
    }

    char *list::read(char *itr, const char *const itr_end)
    {
        static_assert(sizeof(count) == sizeof(int32_t));
        static_assert(sizeof(tag_type_enum) == sizeof(char));

        auto itr_start = itr;

        count = read_var<int32_t>(itr);
        if (count < 0) [[unlikely]] throw std::runtime_error("Found list with negative length while parsing binary NBT data.");
        tags.reserve(count);

        try
        {
            if (tag_properties[type].category & (cat_compound | cat_list))
            {
                if (type == tag_list)
                {
                    for (int32_t index = 0; index < count; index++)
                    {
                        if ((itr + sizeof(tag_type_enum) + padding_size) >= itr_end)
                            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                        auto list_type = static_cast<tag_type_enum>(*itr++);
                        if (static_cast<uint8_t>(list_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

                        tags.push_back(mem::pmr::make_obj_using_pmr<list>(pmr_rsrc, &itr, itr_end, this, mem::pmr::make_pmr_empty_unique<std::pmr::string>(pmr_rsrc), list_type));
                    }
                }
                else if (type == tag_compound)
                {
                    for (int32_t index = 0; index < count; index++)
                    {
                        if ((itr + sizeof(tag_type_enum) + padding_size) >= itr_end)
                            [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                        tags.push_back(mem::pmr::make_obj_using_pmr<compound>(pmr_rsrc, &itr, itr_end, this, mem::pmr::make_pmr_empty_unique<std::pmr::string>(pmr_rsrc)));
                    }
                }
            }
            else if (tag_properties[type].category == cat_primitive)
            {
                for (int32_t index = 0; index < count; index++)
                {
                    if ((itr + tag_properties[type].size + padding_size) >= itr_end)
                        [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

                    tags.push_back(mem::pmr::make_obj_using_pmr<primitive_tag>(pmr_rsrc, type, read_tag_primitive(&itr, type)));
                }
            }
            else if (tag_properties[type].category & (cat_array | cat_string))
            {
                if (type == tag_string)
                {
                    for (int32_t index = 0; index < count; index++)
                    {
                        const auto &[str_ptr, str_len] = read_tag_string(&itr, itr_end, pmr_rsrc);
                        tags.push_back(mem::pmr::make_obj_using_pmr<primitive_tag>(pmr_rsrc, type, std::bit_cast<uint64_t>(str_ptr), nullptr, static_cast<uint32_t>(str_len)));
                    }
                }
                else
                {
                    for (int32_t index = 0; index < count; index++)
                    {
                        auto [array_ptr, array_len] = read_tag_array(&itr, itr_end, type, pmr_rsrc);
                        if (array_len < 0) [[unlikely]] throw std::runtime_error("Found array with negative length while parsing binary NBT data.");

                        tags.push_back(mem::pmr::make_obj_using_pmr<primitive_tag>(pmr_rsrc, type, std::bit_cast<uint64_t>(array_ptr), nullptr, static_cast<uint32_t>(array_len)));
                    }
                }
            }
        }
        catch (...)
        {
            // The above should leave the tag container in a correct state, so all we need to do here is clean it up on our way out.
            switch (tag_properties[type].category)
            {
                case cat_string:
                case cat_array:
                    for (const auto &tag: tags)
                    {
                        pmr_rsrc->deallocate(static_cast<primitive_tag *>(tag)->value.generic_ptr,
                                             tag_properties[type].size * static_cast<primitive_tag *>(tag)->size(), tag_properties[type].size);
                        delete static_cast<primitive_tag *>(tag);
                    }

                    break;
                case cat_primitive:
                    for (const auto &tag: tags)
                        delete static_cast<primitive_tag *>(tag);

                    break;
                case cat_list:
                    for (const auto &tag: tags)
                        delete static_cast<list *>(tag);

                    break;
                case cat_compound:
                    for (const auto &tag: tags)
                        delete static_cast<compound *>(tag);

                    break;
                default:
                    std::unreachable();
            }

            throw;
        }

        size = itr - itr_start;
        return itr;
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
                process_entries.template operator()<list>(tags.data());
            else if (type == tag_compound)
                process_entries.template operator()<compound>(tags.data());
            else
                process_entries.template operator()<primitive_tag>(tags.data());

            out.back() = ']';
        }
        else
            out.push_back(']');
    }

    // Circular dependency hell
    template<>
    std::optional<list *> compound::create<tag_list>(std::string_view tag_name, tag_type_enum tag_type_in, const std::function<void(list *)> &builder)
    {
        if (tags.contains(tag_name)) return std::nullopt;
        if (tag_type_in == tag_end) throw std::runtime_error("Attempted to create NBT list with no type.");

        auto container = mem::pmr::make_pmr_unique<list>(pmr_rsrc, this, tag_name, tag_type_in);

        auto [_, success] = tags.insert(std::pair{ std::string_view(*(container->name)), container.get() });
        if (!success) [[unlikely]] throw std::runtime_error("Failed to insert NBT list.");

        if (builder) builder(container.get());

        return container.release();
    }

    void list::adjust_size(int64_t by)
    {
        if (max_size > -1 && by > -1 && (size + by) > static_cast<uint64_t>(max_size)) [[unlikely]] throw std::runtime_error("NBT compound grew too large.");

        std::visit([by](auto &&tag) {
            tag->adjust_size(by);
        }, parent);

        // Only adjust size after all recursive checks to allow strong exception guarantee.
        size += by;
    }

    void list::remove_container(std::variant<compound *, list *> container)
    {
        std::visit([this](auto &&tag) {
            for (auto itr = tags.begin(); itr < tags.end(); itr++)
            {
                if (*itr == tag)
                {
                    tags.erase(itr);
                    return;
                }
            }
        }, container);
    }
}