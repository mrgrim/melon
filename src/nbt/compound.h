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
#include "util/util.h"
#include "mem/pmr.h"

// TODO: Add binary serialization
// TODO: Add SNBT parsing
// TODO: Region file support

namespace melon::nbt
{
    class list;

    class compound
    {
    public:
        using allocator_type = std::pmr::polymorphic_allocator<>;
        using tag_list_t = std::pmr::unordered_map<std::string_view, std::variant<compound *, list *, primitive_tag *>>;

        // @formatter:off
        class iterator
        {
            friend class compound;

            tag_list_t::iterator itr;
            using itr_value = tag_list_t::iterator::value_type;
        public:
            using value_type = std::tuple<std::string_view, tag_type_enum, tag_variant_t>;
            using difference_type = int;
            using iterator_category = std::forward_iterator_tag;

            iterator() : itr() { };

            explicit iterator(tag_list_t::iterator &itr_in) : itr(itr_in) { };
            explicit iterator(tag_list_t::iterator &&itr_in) : itr(itr_in) { };

            value_type operator*() const { return fetch_value(*itr); };
            auto &operator++() { itr++; return *this; };
            auto operator++(int) { auto old = *this; ++(*this); return old; };

            friend bool operator==(const iterator &lhs, const iterator &rhs) { return lhs.itr == rhs.itr; }

        private:
            static value_type fetch_value(itr_value &value_in);
        };

        class const_iterator
        {
            friend class compound;

            tag_list_t::const_iterator itr;
            using itr_value = tag_list_t::const_iterator::value_type;
        public:
            using value_type = std::tuple<const std::string_view, const tag_type_enum, const tag_variant_t>;
            using difference_type = int;
            using iterator_category = std::forward_iterator_tag;

            const_iterator() : itr() { };

            explicit const_iterator(tag_list_t::const_iterator &itr_in) : itr(itr_in) { };
            explicit const_iterator(tag_list_t::iterator &itr_in) : itr(itr_in) { };

            explicit const_iterator(tag_list_t::const_iterator &&itr_in) : itr(itr_in) { };
            explicit const_iterator(tag_list_t::iterator &&itr_in) : itr(itr_in) { };

            value_type operator*() const { return fetch_value(*itr); };
            auto &operator++() { itr++; return *this; };
            auto operator++(int) { auto old = *this; ++(*this); return old; };

            friend bool operator==(const const_iterator &lhs, const const_iterator &rhs) { return lhs.itr == rhs.itr; }

        private:
            static value_type fetch_value(const itr_value &value_in);
        };
        //@formatter:on

        class compound_node_handle
        {
            friend class compound;

            compound::tag_list_t::node_type tag_node{ };

            explicit compound_node_handle(compound::tag_list_t::node_type &&tag_node_in)
                    : tag_node(std::move(tag_node_in))
            { }

        public:
            using allocator_type = compound::tag_list_t::node_type::allocator_type;
            using key_type = compound::tag_list_t::node_type::key_type;
            using mapped_type = tag_variant_t;

            compound_node_handle() noexcept = default;
            compound_node_handle(compound_node_handle &node_in) = delete;

            compound_node_handle(compound_node_handle &&node_in) noexcept
                    : tag_node(std::move(node_in.tag_node))
            { }

            compound_node_handle &operator=(compound_node_handle &&node_in) noexcept
            {
                tag_node = std::move(node_in.tag_node);
                return *this;
            }

            ~compound_node_handle() = default;

            [[nodiscard]] bool empty() const noexcept
            { return tag_node.empty(); }

            explicit operator bool() const noexcept
            { return !tag_node.empty(); }

            [[nodiscard]] allocator_type get_allocator() const
            { return tag_node.get_allocator(); }

            [[nodiscard]] key_type &key() const
            { return tag_node.key(); }

            [[nodiscard]] mapped_type mapped() const
            {
                if (std::holds_alternative<compound *>(tag_node.mapped()))
                    return std::reference_wrapper<compound>(*std::get<compound *>(tag_node.mapped()));
                else if (std::holds_alternative<list *>(tag_node.mapped()))
                    return std::reference_wrapper<list>(*std::get<list *>(tag_node.mapped()));
                else if (std::holds_alternative<primitive_tag *>(tag_node.mapped()))
                {
                    auto prim_ptr = std::get<primitive_tag *>(tag_node.mapped());
                    return prim_ptr->get_generic();
                }
                else
                    std::unreachable();
            }

            void swap(compound_node_handle &node_in) noexcept
            { return tag_node.swap(node_in.tag_node); }

            friend void swap(compound_node_handle &x, compound_node_handle &y) noexcept
            { x.swap(y); }
        };

        struct compound_insert_result
        {
            compound::iterator   position;
            bool                 inserted;
            compound_node_handle node;
        };

    private:
        using node_type = compound_node_handle;
        using insert_return_type = compound_insert_result;

        struct insert_args : util::forced_named_init<insert_args>
        {
            bool overwrite = false;
        };

        std::variant<compound *, list *> parent;
        compound                         *top;
        std::pmr::memory_resource        *pmr_rsrc;

        // @formatter:off
        template<template<class> class Ref, class Cont>
        requires (std::is_same_v<Cont, compound> || std::is_same_v<Cont, list>) && std::is_same_v<std::reference_wrapper<Cont>, Ref<Cont>>
        class chained_optional_refwrap : public std::optional<Ref<Cont>>
        {
            using std::optional<Ref<Cont>>::optional;

            public:
            template<tag_type_enum tag_type>
            requires std::is_same_v<Cont, compound>
            chained_optional_refwrap<Ref, tag_cont_t<tag_type>> find(const std::string_view &tag_name)
            {
                if (this->has_value())
                    return this->value().get().template find<tag_type>(tag_name);
                else
                    return std::nullopt;
            }
        };
        // @formatter:on

    public:
        mem::pmr::unique_ptr<std::pmr::string> name;

        compound() = delete;

        // For building a compound from scratch
        explicit compound(std::string_view name_in, const allocator_type &alloc = { })
            : compound(name_in, -1, nullptr, alloc)
        { }

        explicit compound(std::string_view name_in, int64_t max_size_in, const allocator_type &alloc = { })
            : compound(name_in, max_size_in, nullptr, alloc)
        { }

        explicit compound(std::string_view name_in, const std::function<void(compound &)> &builder, const allocator_type &alloc = { })
            : compound(name_in, -1, builder, alloc)
        { }

        explicit compound(std::string_view name_in, int64_t max_size_in, const std::function<void(compound &)> &builder, const allocator_type &alloc = { });

        // For parsing a binary NBT buffer
        // This function expects the raw buffer provided to it to be at least 8 bytes larger than the NBT data. The deflate methods in melon::util
        // will take care of this automatically.
        explicit compound(std::unique_ptr<char[]> raw, size_t raw_size, const allocator_type &alloc = { });

        iterator begin()
        { return iterator(tags.begin()); }

        iterator end()
        { return iterator(tags.end()); }

        const_iterator cbegin()
        { return const_iterator(tags.cbegin()); }

        const_iterator cend()
        { return const_iterator(tags.cend()); }

        template<tag_type_enum tag_type>
        requires is_nbt_container<tag_type>
        [[nodiscard]] chained_optional_refwrap<std::reference_wrapper, tag_cont_t<tag_type>> find(const std::string_view &tag_name) noexcept
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return *std::get<tag_cont_t<tag_type> *>(itr->second);
        }

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        [[nodiscard]] std::optional<std::reference_wrapper<tag_prim_t<tag_type>>> find(const std::string_view &tag_name) noexcept
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::get<tag_cont_t<tag_type> *>(itr->second)->template get<tag_type>();
        }

        template<tag_type_enum tag_type>
        requires is_nbt_array<tag_type>
        [[nodiscard]] std::optional<typename std::invoke_result<decltype(&primitive_tag::template get<tag_type>), primitive_tag *>::type>
        find(const std::string_view &tag_name) noexcept
        {
            auto itr = tags.find(tag_name);

            if (itr == tags.end() || !std::holds_alternative<tag_cont_t<tag_type> *>(itr->second))
                return std::nullopt;
            else
                return std::get<tag_cont_t<tag_type> *>(itr->second)->template get<tag_type>();
        }

        std::optional<std::tuple<std::string_view, tag_type_enum, tag_variant_t>> find(const std::string_view &key, tag_type_enum type_requested = tag_end) noexcept;

        template<tag_type_enum tag_type>
        requires (tag_type == tag_compound)
        std::optional<std::reference_wrapper<compound>> create(std::string_view tag_name, const std::function<void(compound &)> &builder = nullptr)
        {
            if (tags.contains(tag_name)) return std::nullopt;
            auto container = mem::pmr::make_unique<compound>(pmr_rsrc, this, tag_name);

            try
            {
                if (builder) builder(*container);
                const auto &[_, success] = tags.insert(std::pair{ std::string_view(*(container->name)), container.get() });
                if (!success) throw std::runtime_error("Failed to insert NBT compound.");
            }
            catch (...)
            {
                this->adjust_byte_count(container->bytes() * -1);
                throw;
            }

            return *container.release();
        }

        // It's in list.cpp :sob:
        template<tag_type_enum tag_type>
        requires (tag_type == tag_list)
        std::optional<std::reference_wrapper<list>> create(std::string_view tag_name, tag_type_enum tag_type_in, const std::function<void(list &)> &builder = nullptr);

        template<tag_type_enum tag_type>
        requires is_nbt_container<tag_type>
        std::pair<iterator, bool> insert(tag_cont_t<tag_type> *container, insert_args args = { .overwrite = false })
        {
            auto found = tags.find(std::string_view(*container->name));

            if (found == tags.end() || args.overwrite)
            {
                node_type found_node;
                if (found == tags.end()) found_node = extract(const_iterator(found));

                try
                {
                    adjust_byte_count(container->bytes());
                    if ((depth + container->get_tree_depth()) > 512) throw std::runtime_error("Inserting NBT container would exceed maximum depth (>512).");

                    auto &&[itr, success] = tags.insert(std::pair{ std::string_view(*container->name), container });
                    container->change_properties({ .new_depth = depth + 1, .new_max_bytes = max_bytes, .new_parent = this, .new_top = top });

                    return { iterator(std::move(itr)), success };
                }
                catch (...)
                {
                    adjust_byte_count(container->bytes() * -1);
                    insert(std::move(found_node)); // Since we just extracted it there should be room to put it back without throwing... right?
                    throw;
                }
            }

            return { iterator(tags.end()), false };
        }

        template<tag_type_enum tag_type, is_nbt_type_match<tag_type> V>
        requires is_nbt_primitive<tag_type>
        std::pair<iterator, bool> insert(const std::string_view tag_name, V value, insert_args args = { .overwrite = false })
        {
            if (tag_name.size() >= std::numeric_limits<uint16_t>::max())
                [[unlikely]] throw std::runtime_error("Attempted to add nbt primitive tag with too large name to NBT compound.");

            auto [name_ptr, tag_ptr] = new_primitive(tag_name, tag_type, args.overwrite);

            if constexpr (tag_type == tag_byte)
                tag_ptr->value.tag_byte = value;
            else if constexpr (tag_type == tag_short)
                tag_ptr->value.tag_short = value;
            else if constexpr (tag_type == tag_int)
                tag_ptr->value.tag_int = value;
            else if constexpr (tag_type == tag_long)
                tag_ptr->value.tag_long = value;
            else if constexpr (tag_type == tag_float)
                tag_ptr->value.tag_float = value;
            else if constexpr (tag_type == tag_double)
                tag_ptr->value.tag_double = value;

            this->adjust_byte_count(tag_ptr->bytes());

            auto &&[itr, success] = tags.insert(std::pair{ std::string_view(*name_ptr), tag_ptr.get() });

            static_cast<void>(name_ptr.release());
            static_cast<void>(tag_ptr.release());

            return { iterator(itr), success };
        }

        template<tag_type_enum tag_type>
        requires (tag_type == tag_string)
        std::pair<iterator, bool> insert(const std::string_view tag_name, const std::string_view value, insert_args args = { .overwrite = false })
        { return insert_array_general<tag_type>(tag_name, value, args.overwrite); }

        template<tag_type_enum tag_type, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, std::size_t N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type>
        std::pair<iterator, bool> insert(const std::string_view tag_name, const std::array<V, N> &values, insert_args args = { .overwrite = false })
        { return insert_array_general<tag_type>(tag_name, values, args.overwrite); }

        template<tag_type_enum tag_type, template<class, class...> class C = std::initializer_list, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, class... N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type> && util::is_simple_iterable<C<V, N...>, V>
        std::pair<iterator, bool> insert(const std::string_view tag_name, const C<V, N...> &values, insert_args args = { .overwrite = false })
        { return insert_array_general<tag_type>(tag_name, values, args.overwrite); }

        insert_return_type insert(node_type &&node_in);

        node_type extract(const const_iterator &pos);
        node_type extract(const std::string_view &key, tag_type_enum type_requested = tag_end);

        bool contains(const std::string_view key)
        { return tags.contains(key); }

        void merge(compound &src);

        iterator erase(const iterator &pos)
        { return iterator(destroy_tag(pos.itr)); }

        size_t erase(std::string_view &&key);

        void to_snbt(std::string &out);
        std::unique_ptr<std::string> to_snbt();

        [[nodiscard]] size_t bytes() const
        { return byte_count_v; }

        [[nodiscard]] size_t size() const
        { return tags.size(); }

        uint16_t get_tree_depth();
        void clear();

        compound(const compound &) = delete;
        compound &operator=(const compound &) = delete;

        compound(compound &&) = delete;
        compound &operator=(compound &&) = delete;

        ~compound();
    private:
        friend class list;

        template<class T, class... Args>
        friend auto mem::pmr::make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
        requires (!std::is_array_v<T>);

        explicit compound(std::variant<compound *, list *> parent_in, std::string_view name_in);
        explicit compound(char **itr_in, const char *itr_end, std::variant<compound *, list *> parent_in, mem::pmr::unique_ptr<std::pmr::string> name_in);

        std::pair<mem::pmr::unique_ptr<std::pmr::string>, mem::pmr::unique_ptr<primitive_tag>> new_primitive(std::string_view, tag_type_enum, bool overwrite = false);
        char *read(char *itr, const char *itr_end);
        void adjust_byte_count(int64_t by);
        tag_list_t::iterator destroy_tag(const tag_list_t::iterator &itr);
        void destroy_tag(std::variant<compound *, list *, primitive_tag *> &tag_variant);
        void change_properties(container_property_args props);

        template<tag_type_enum tag_type, class V = std::remove_pointer_t<tag_prim_t<tag_type>>>
        requires is_nbt_type_match<V *, tag_type> && is_nbt_array<tag_type>
        std::pair<iterator, bool> insert_array_general(const std::string_view tag_name, const auto &values, bool overwrite = false)
        {
            if (tag_name.size() >= std::numeric_limits<uint16_t>::max())
                [[unlikely]] throw std::runtime_error("Attempted to add array tag with too large name to NBT compound.");

            if ((tag_type == tag_string && values.size() >= std::numeric_limits<uint16_t>::max()) || (values.size() >= std::numeric_limits<int32_t>::max()))
                [[unlikely]] throw std::runtime_error("Attempted to add too large array tag to NBT compound.");

            auto [name_ptr, tag_ptr] = new_primitive(tag_name, tag_type, overwrite);
            auto array_ptr           = mem::pmr::make_unique<V[]>(pmr_rsrc, values.size() + (padding_size / sizeof(V)));

            tag_ptr->set_size(values.size());
            this->adjust_byte_count(tag_ptr->bytes());

            if constexpr (tag_type == tag_string)
                tag_ptr->value.tag_string     = array_ptr.get();
            if constexpr (tag_type == tag_byte_array)
                tag_ptr->value.tag_byte_array = array_ptr.get();
            if constexpr (tag_type == tag_int_array)
                tag_ptr->value.tag_int_array  = array_ptr.get();
            if constexpr (tag_type == tag_long_array)
                tag_ptr->value.tag_long_array = array_ptr.get();

            if constexpr (requires(decltype(values) v) { v.data(); v.size(); })
                std::memcpy(static_cast<void *>(array_ptr.get()), static_cast<const void *>(values.data()), values.size() * sizeof(V));
            else
                for (uint32_t idx = 0; auto &&value: values)
                    array_ptr[idx++] = value;

            auto &&[itr, success] = tags.insert(std::pair{ std::string_view(*name_ptr), tag_ptr.get() });

            static_cast<void>(name_ptr.release());
            static_cast<void>(array_ptr.release());
            static_cast<void>(tag_ptr.release());

            return { iterator(itr), success };
        }

        tag_list_t tags;

        uint16_t depth        = 0;
        size_t   byte_count_v = 0;
        int64_t  max_bytes    = -1;
    };

    static_assert(std::forward_iterator<compound::iterator>);
    static_assert(std::forward_iterator<compound::const_iterator>);
}

#endif //MELON_NBT_COMPOUND_H
