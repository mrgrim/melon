//
// Created by MrGrim on 8/14/2022.
//

#include <cstring>
#include <string_view>
#include <memory>
#include <memory_resource>
#include "nbt.h"
#include "list.h"
#include "compound.h"
#include "mem/pmr.h"

// For details on the file format go to: https://minecraft.fandom.com/wiki/NBT_format#Binary_format

namespace melon::nbt
{
    compound::compound(std::string_view name_in, int64_t max_size_in, std::pmr::memory_resource *pmr_rsrc_in)
            : parent(std::nullopt),
              top(this),
              pmr_rsrc(new mem::pmr::recording_mem_resource(pmr_rsrc_in)),
              name(mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, name_in)),
              tags(tag_list_t(pmr_rsrc)),
              depth(1),
              max_size(max_size_in)
    {
        if (name_in.size() > std::numeric_limits<uint16_t>::max())
            [[unlikely]]
                    throw std::runtime_error("Attempted to create NBT compound with too large name.");

        // TAG type + name length value + name length + END TAG type
        this->adjust_size(sizeof(int8_t) + sizeof(uint16_t) + name_in.size() + sizeof(int8_t));
    }

    compound::compound(std::unique_ptr<char[]> raw, size_t raw_size, std::pmr::memory_resource *pmr_rsrc_in)
            : parent(std::nullopt),
              top(this),
              pmr_rsrc(new mem::pmr::recording_mem_resource(pmr_rsrc_in)),
              name(mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, "")),
              tags(tag_list_t(pmr_rsrc)),
              depth(1),
              max_size(-1)
    {
        if (raw_size < 5) [[unlikely]] throw std::runtime_error("NBT Compound Tag Too Small.");

        auto itr = raw.get();
        if (static_cast<tag_type_enum>(*itr++) != tag_compound) [[unlikely]] throw std::runtime_error("NBT tag type not compound.");

        auto name_len = read_var<uint16_t>(itr);
        *name = std::string_view(itr, name_len);

        itr += name_len;
        read(itr, raw.get() + raw_size);
    }

    compound::compound(std::variant<compound *, list *> parent_in, std::string_view name_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              name(mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, name_in)),
              tags(tag_list_t(pmr_rsrc))
    {
        if (name_in.size() > std::numeric_limits<uint16_t>::max()) [[unlikely]] throw std::runtime_error("Attempted to create NBT compound with too large name.");

        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        if (depth > 512) [[unlikely]] throw std::runtime_error("NBT Depth exceeds 512.");

        // TAG type + name length value + name length + END TAG type
        this->adjust_size(sizeof(int8_t) + sizeof(uint16_t) + name_in.size() + sizeof(int8_t));
    }

    compound::compound(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, mem::pmr::unique_ptr<std::pmr::string> name_in)
            : parent(parent_in),
              top(std::visit([](auto &&tag) -> compound * { return tag->top; }, parent_in)),
              pmr_rsrc(top->pmr_rsrc),
              name(std::move(name_in)),
              tags(tag_list_t(pmr_rsrc))
    {
        std::visit([this](auto &&tag) {
            depth    = tag->depth + 1;
            max_size = tag->max_size;
        }, parent_in);

        if (depth > 512) [[unlikely]] throw std::runtime_error("NBT Depth exceeds 512.");

        pmr_rsrc->start_recording();

        try
        {
            *itr_in = read(*itr_in, itr_end);
        }
        catch (...)
        {
            // Just dump the entire structure under us to provide strong guarantee.
            pmr_rsrc->deallocate_recorded();
            pmr_rsrc->stop_recording();
            throw;
        }

        pmr_rsrc->stop_recording();
    }

    compound::~compound()
    {
        for (auto itr = tags.begin(); itr != tags.end();)
        {
            auto &[key, tag] = *itr;
            std::visit([this](auto &&tag_in) {
                if constexpr (std::is_same_v<std::decay_t<decltype(tag_in)>, primitive_tag *>)
                {
                    if (tag_properties[tag_in->tag_type].category & (cat_array | cat_string))
                    {
                        pmr_rsrc->deallocate(tag_in->value.generic_ptr, tag_in->size() * tag_properties[tag_in->tag_type].size + padding_size,
                                             tag_properties[tag_in->tag_type].size);
                    }

                    std::destroy_at(tag_in);
                    pmr_rsrc->deallocate(tag_in, sizeof(std::remove_pointer_t<decltype(tag_in)>), alignof(std::remove_pointer_t<decltype(tag_in)>));
                }
            }, tag);

            itr = tags.erase(itr);
        }
    }

    char *compound::read(char *itr, const char *const itr_end)
    {
        static_assert(sizeof(tag_type_enum) == sizeof(std::byte));
        auto     itr_start = itr;

        auto tag_type = static_cast<tag_type_enum>(*itr++);
        if (static_cast<uint8_t>(tag_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT Tag Type.");

        while ((itr_end - itr) >= 2 && tag_type != tag_end)
        {
            auto name_len = read_var<uint16_t>(itr);

            if ((itr + name_len + padding_size) >= itr_end)
                [[unlikely]] throw std::runtime_error("Attempt to read past buffer while parsing binary NBT data.");

            auto tag_name = mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, itr, name_len);
            auto tag_name_ptr = tag_name.get();
            itr += name_len;

            auto create_and_insert = [this, &tag_name_ptr]<class T, class ...Args>(Args &&... args) {
                auto tag_ptr = mem::pmr::make_obj_using_pmr<T>(pmr_rsrc, std::forward<Args>(args)...);
                const auto &[_, success] = tags.insert(std::pair{ std::string_view(*tag_name_ptr), tag_ptr });
                if (!success) throw std::runtime_error("Unable to insert NBT tag to compound (possible duplicate).");
            };

            if (tag_properties[tag_type].category & (cat_compound | cat_list))
            {
                if (tag_type == tag_list)
                {
                    auto list_type = static_cast<tag_type_enum>(*itr++);
                    if (static_cast<uint8_t>(list_type) >= tag_properties.size()) [[unlikely]] throw std::runtime_error("Invalid NBT tag type while initializing list.");

                    create_and_insert.template operator()<list>(&itr, itr_end, this, std::move(tag_name), list_type);
                }
                else if (tag_type == tag_compound)
                {
                    create_and_insert.template operator()<compound>(&itr, itr_end, this, std::move(tag_name));
                }
            }
            else if (tag_properties[tag_type].category == cat_primitive)
            {
                create_and_insert.template operator()<primitive_tag>(tag_type, read_tag_primitive(&itr, tag_type), tag_name.get());
                static_cast<void>(tag_name.release());
            }
            else if (tag_properties[tag_type].category & (cat_array | cat_string))
            {
                if (tag_type == tag_string)
                {
                    const auto &[str_ptr, str_len] = read_tag_string(&itr, itr_end, pmr_rsrc);
                    create_and_insert.template operator()<primitive_tag>(tag_type, std::bit_cast<uint64_t>(str_ptr), tag_name.get(), static_cast<uint32_t>(str_len));
                    static_cast<void>(tag_name.release());
                }
                else
                {
                    const auto &[array_ptr, array_len] = read_tag_array(&itr, itr_end, tag_type, pmr_rsrc);
                    if (array_len < 0) [[unlikely]] throw std::runtime_error("Found array with negative length while parsing binary NBT data.");

                    create_and_insert.template operator()<primitive_tag>(tag_type, std::bit_cast<uint64_t>(array_ptr), tag_name.get(), static_cast<uint32_t>(array_len));
                    static_cast<void>(tag_name.release());
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

        for (const auto &[_, tag]: tags)
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

    std::unique_ptr<std::string> compound::to_snbt()
    {
        auto out = std::make_unique<std::string>();
        this->to_snbt(*out);
        return out;
    }

    std::pair<primitive_tag *, bool> compound::get_primitive(std::string_view tag_name, tag_type_enum tag_type, bool overwrite)
    {
        auto          itr    = tags.find(tag_name);
        primitive_tag *tag_ptr;
        bool          is_new = false;

        if (itr == tags.end())
        {
            auto str_ptr = mem::pmr::make_pmr_unique<std::pmr::string>(pmr_rsrc, tag_name);
            auto tag_ptr_u = mem::pmr::make_pmr_unique<primitive_tag>(pmr_rsrc, tag_type, 0, str_ptr.get());

            const auto &[_, success] = tags.insert(std::pair{ std::string_view(*str_ptr), tag_ptr_u.get() });
            if (!success) throw std::runtime_error("Failed to insert new tag into NBT compound.");

            is_new = true;
            static_cast<void>(str_ptr.release());
            tag_ptr = tag_ptr_u.release();
        }
        else if (overwrite)
        {
            if (!(std::holds_alternative<primitive_tag *>(itr->second) &&
                  (tag_ptr = std::get<primitive_tag *>(itr->second))->tag_type == tag_type))
                [[unlikely]] throw std::runtime_error("Attempted to write wrong type to NBT tag.");
        }
        else
        {
            throw std::runtime_error("Attempted to insert over existing key in NBT compound.");
        }

        return { tag_ptr, is_new };
    }

    // Circular dependency hell
    template<>
    std::optional<compound *> list::push<tag_compound>(const std::function<void(compound *)> &builder)
    {
        auto container = mem::pmr::make_pmr_unique<compound>(pmr_rsrc, this, "");

        if (builder) builder(container.get());

        auto cont_ptr = container.get();
        tags.push_back(std::move(container));

        count++;
        return cont_ptr;
    }

    void compound::adjust_size(int64_t by)
    {
        if (max_size > -1 && by > -1 && (size + by) > static_cast<uint64_t>(max_size)) [[unlikely]] throw std::runtime_error("NBT compound grew too large.");

        if (parent.has_value())
        {
            std::visit([by](auto &&tag) {
                tag->adjust_size(by);
            }, parent.value());
        }

        // Only adjust size after all recursive checks to allow strong exception guarantee.
        size += by;
    }
}