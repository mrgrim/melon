//
// Created by MrGrim on 8/19/2022.
//

#ifndef MELON_NBT_COMPOUND_H
#define MELON_NBT_COMPOUND_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <memory_resource>
#include <iterator>
#include "nbt.h"
#include "unordered_dense.h"
#include "util/concepts.h"
#include "mem/pmr.h"

namespace melon::nbt
{
    class list;

    class compound
    {
    private:
        struct insert_args : util::forced_named_init<insert_args> {
            bool overwrite = false;
        };

        std::optional<std::variant<compound *, list *>> parent;
        compound                                        *top;
        mem::pmr::recording_mem_resource                *pmr_rsrc;

    public:
        using allocator_type = std::pmr::polymorphic_allocator<std::byte>;
        using tag_list_t = std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>>;

        mem::pmr::unique_ptr<std::pmr::string> name;

        compound() = delete;

        // For building a compound from scratch
        explicit compound(std::string_view name_in = "", int64_t max_size_in = -1, std::pmr::memory_resource *pmr_rsrc_in = std::pmr::get_default_resource());

        // For parsing a binary NBT buffer
        // This function expects the raw buffer provided to it to be at least 8 bytes larger than the NBT data. The deflate methods in melon::util
        // will take care of this automatically.
        explicit compound(std::unique_ptr<char[]> raw, size_t raw_size, std::pmr::memory_resource *pmr_rsrc_in = std::pmr::get_default_resource());

        template<tag_type_enum tag_type>
        requires is_nbt_container<tag_type>
        std::optional<tag_cont_t<tag_type> *> get(const std::string_view &tag_name) noexcept
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::get<tag_cont_t<tag_type> *>(itr->second);
        }

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        std::optional<std::reference_wrapper<tag_prim_t<tag_type>>> get(const std::string_view &tag_name) noexcept
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::reference_wrapper<tag_prim_t<tag_type>>(std::get<tag_cont_t<tag_type> *>(itr->second)->template get<tag_type>());
        }

        template<tag_type_enum tag_type>
        requires is_nbt_array<tag_type>
        std::optional<typename std::invoke_result<decltype(&primitive_tag::template get<tag_type>), primitive_tag *>::type> get(const std::string_view &tag_name)
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::get<tag_cont_t<tag_type> *>(itr->second)->template get<tag_type>();
        }

        template<tag_type_enum tag_type>
        requires (tag_type == tag_compound)
        std::optional<compound *> create(std::string_view tag_name, const std::function<void(compound *)> &builder = nullptr)
        {
            if (tags.contains(tag_name)) return std::nullopt;

            auto container = mem::pmr::make_pmr_unique<compound>(pmr_rsrc, this, tag_name);

            auto [itr, success] = tags.insert(std::pair{ std::string_view(*(container->name)), container.get() });
            if (!success) throw std::runtime_error("Failed to insert NBT compound.");

            if (builder) builder(std::get<compound *>(itr->second));

            return container.release();
        }

        // It's in list.cpp :sob:
        template<tag_type_enum tag_type>
        requires (tag_type == tag_list)
        std::optional<list *> create(std::string_view tag_name, tag_type_enum tag_type_in, const std::function<void(list *)> &builder = nullptr);

        // There is no insert for container types. You can get one or create one only.
        template<tag_type_enum tag_type, is_nbt_type_match<tag_type> V>
        requires is_nbt_primitive<tag_type>
        void insert(const std::string_view tag_name, V value, insert_args args = { .overwrite = false })
        {
            if (tag_name.size() >= std::numeric_limits<uint16_t>::max())
                [[unlikely]] throw std::runtime_error("Attempted to add nbt primitive tag with too large name to NBT compound.");

            auto [tag_ptr, is_new] = get_primitive(tag_name, tag_type, args.overwrite);
            std::memcpy(static_cast<void *>(&(tag_ptr->value)), static_cast<void *>(&value), sizeof(V));

            if (is_new) this->adjust_size(sizeof(uint16_t) + tag_name.size() + sizeof(V));
        }

        template<tag_type_enum tag_type>
        requires (tag_type == tag_string)
        void insert(const std::string_view tag_name, const std::string_view value, insert_args args = { .overwrite = false })
        {
            if (tag_name.size() >= std::numeric_limits<uint16_t>::max() || value.size() >= std::numeric_limits<uint16_t>::max())
                [[unlikely]] throw std::runtime_error("Attempted to add to large string tag to NBT compound.");

            auto [tag_ptr, is_new] = get_primitive(tag_name, tag_type, args.overwrite);
            tag_ptr->value.tag_string = static_cast<char *>(pmr_rsrc->allocate(value.size() + padding_size, alignof(char)));
            std::memcpy(tag_ptr->value.tag_string, value.data(), value.size());

            if (is_new) this->adjust_size(sizeof(uint16_t) + value.size() + sizeof(uint16_t) + value.size());
            else this->adjust_size(value.size() - tag_ptr->size());

            tag_ptr->resize(value.size());
        }

        template<tag_type_enum tag_type, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, std::size_t N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type>
        void insert(const std::string_view tag_name, const std::array<V, N> &values, insert_args args = { .overwrite = false })
        {
            insert_array_general<V>(tag_name, tag_type, values, args.overwrite);
        }

        template<tag_type_enum tag_type, template<class, class...> class C = std::initializer_list, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, class... N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type> && util::is_simple_iterable<C<V, N...>, V>
        void insert(const std::string_view tag_name, const C<V, N...> &values, insert_args args = { .overwrite = false })
        {
            insert_array_general<V>(tag_name, tag_type, values, args.overwrite);
        }

        void to_snbt(std::string &out);
        std::unique_ptr<std::string> to_snbt();

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) = delete;
        compound &operator=(compound &&) = delete;

        ~compound();
    private:
        friend class list;

        template<class T, class... Args>
        friend auto mem::pmr::make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args&&... args)
        requires (!std::is_array_v<T>);

        explicit compound(std::variant<compound *, list *> parent_in, std::string_view name_in);
        explicit compound(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, mem::pmr::unique_ptr<std::pmr::string> name_in);

        std::pair<primitive_tag *, bool> get_primitive(std::string_view, tag_type_enum, bool overwrite = false);
        char *read(char *itr, const char *itr_end);
        void adjust_size(int64_t by);

        template<typename V>
        void insert_array_general(const std::string_view tag_name, tag_type_enum tag_type, const auto &values, bool overwrite = false)
        {
            if (tag_name.size() >= std::numeric_limits<uint16_t>::max() || values.size() >= std::numeric_limits<int32_t>::max())
                [[unlikely]] throw std::runtime_error("Attempted to add to large array tag to NBT compound.");

            auto [tag_ptr, is_new] = get_primitive(tag_name, tag_type, overwrite);

            if (tag_ptr->size() < values.size() || tag_ptr->value.generic == 0)
            {
                if (tag_ptr->value.generic != 0) pmr_rsrc->deallocate(tag_ptr->value.generic_ptr, sizeof(V) * tag_ptr->size() + padding_size, tag_properties[tag_type].size);
                tag_ptr->value.generic_ptr = pmr_rsrc->allocate(sizeof(V) * values.size() + padding_size, tag_properties[tag_type].size);
            }

            if (is_new) this->adjust_size(sizeof(uint16_t) + tag_name.size() + sizeof(int32_t) + (values.size() * sizeof(V)));
            else this->adjust_size((values.size() - tag_ptr->size()) * sizeof(V));

            tag_ptr->resize(values.size());

            for (int idx = 0; const auto &value: values)
                static_cast<V *>(tag_ptr->value.generic_ptr)[idx++] = value;
        }

        tag_list_t tags;

        uint16_t depth    = 0;
        size_t   size     = 0;
        int64_t  max_size = -1;
    };
}

#endif //MELON_NBT_COMPOUND_H
