//
// Created by MrGrim on 8/19/2022.
//

#ifndef MELON_NBT_LIST_H
#define MELON_NBT_LIST_H

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include <memory_resource>
#include <cassert>
#include <functional>
#include "nbt.h"
#include "util/concepts.h"

namespace melon::nbt
{
    class compound;

    class list
    {
    public:
        // @formatter:off
        template<tag_type_enum tag_type>
        requires (tag_type != tag_end)
        class iterator
        {
            tag_cont_t<tag_type> *const * ptr;
            int     size;
            int     index;
        public:
            using value_type = tag_iter_t<tag_type>;
            using difference_type = int;
            using iterator_category = std::random_access_iterator_tag;

            iterator() : ptr(nullptr), size(0), index(0) { };

            explicit iterator(void *ptr_in, int size_in, int index_in = 0) : ptr(static_cast<tag_cont_t<tag_type> *const *>(ptr_in)), size(size_in), index(index_in) { };

            //const tag_prim_t<tag_type> *operator->() const { tag_prim_t<tag_type> ret; std::memcpy(&ret, &(ptr[index]->value)); };
            auto &operator*() const requires is_nbt_primitive<tag_type> { return operator[](index); };
            auto &operator[](int idx) const requires is_nbt_primitive<tag_type> { return ptr[idx]->template get<tag_type>(); };

            // Hopefully force return elision of the string_view or span generated by the primitive get() for array types
            auto operator*() const requires is_nbt_array<tag_type> { return operator[](index); };
            auto operator[](int idx) const requires is_nbt_array<tag_type> { return ptr[idx]->template get<tag_type>(); };

            auto operator*() const requires is_nbt_container<tag_type> { return operator[](index); };
            auto operator[](int idx) const requires is_nbt_container<tag_type> { return ptr[idx]; };

            auto &operator++() { index++; return *this; };
            auto operator++(int) { auto old = *this; ++(*this); return old; };
            auto &operator--() { index--; return *this; };
            auto operator--(int) { auto old = *this; --(*this); return old; };
            auto &operator+=(int diff) { index += diff; return *this; };
            auto &operator-=(int diff) { index -= diff; return *this; };

            friend std::strong_ordering operator<=>(const iterator<tag_type> &lhs, const iterator<tag_type> &rhs) { return lhs.index <=> rhs.index; }
            friend bool operator==(const iterator<tag_type> &lhs, const iterator<tag_type> &rhs) { return (lhs.ptr == rhs.ptr) && (lhs.index == rhs.index); }

            friend int operator-(iterator<tag_type> lhs, iterator<tag_type> rhs) { return rhs.index - lhs.index; };
            friend iterator<tag_type> operator+(iterator<tag_type> itr, int diff) { return iterator<tag_type>(itr.ptr, itr.size, itr.index + diff); };
            friend iterator<tag_type> operator-(iterator<tag_type> itr, int diff) { return iterator<tag_type>(itr.ptr, itr.size, itr.index - diff); };
            friend iterator<tag_type> operator+(int diff, iterator<tag_type> itr) { return itr + diff; };
        };
        //@formatter:on

    private:

        std::variant<compound *, list *> parent;
        compound                         *top;
        std::pmr::memory_resource        *pmr_rsrc = std::pmr::get_default_resource();

    public:
        using allocator_type = std::pmr::polymorphic_allocator<std::byte>;
        using tag_list_t = std::pmr::vector<void *>;

        mem::pmr::unique_ptr<std::pmr::string> name;
        const tag_type_enum                    type  = tag_end;
        int32_t                                count = 0;

        list() = delete;

        list(const list &) = delete;
        list &operator=(const list &) = delete;

        list(list &&) = delete;
        list &operator=(list &&) = delete;

        void to_snbt(std::string &out);

        // @formatter:off
        template<tag_type_enum tag_type> requires (tag_type != tag_end)
        iterator<tag_type> begin()
        {
            if (type != tag_type) [[unlikely]] throw std::runtime_error("Attempt to create iterator of invalid NBT list type.");
            return iterator<tag_type>(tags.data(), tags.size());
        }

        template<tag_type_enum tag_type> requires (tag_type != tag_end)
        iterator<tag_type> end()
        {
            if (type != tag_type) [[unlikely]] throw std::runtime_error("Attempt to create iterator of invalid NBT list type.");
            return iterator<tag_type>(tags.data(), tags.size(), tags.size());
        }

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        auto &at(int idx)
        {
            if (type != tag_type) [[unlikely]] throw std::runtime_error("Attempted access of invalid NBT list type element.");
            return (static_cast<tag_cont_t<tag_type> *>(tags.at(idx)))->template get<tag_type>();
        }

        template<tag_type_enum tag_type>
        requires is_nbt_array<tag_type> || is_nbt_container<tag_type>
        auto at(int idx)
        {
            if (type != tag_type) [[unlikely]] throw std::runtime_error("Attempted access of invalid NBT list type element.");

            if constexpr (tag_properties[tag_type].category & (cat_compound | cat_list))
                return static_cast<tag_cont_t<tag_type> *>(tags.at(idx));
            else
                return (static_cast<tag_cont_t<tag_type> *>(tags.at(idx)))->template get<tag_type>();
        }
        // @formatter:on

        // It's in compound.cpp :sob:
        template<tag_type_enum tag_type>
        requires (tag_type == tag_compound)
        std::optional<compound *> push(const std::function<void(compound *)> &builder = nullptr);

        template<tag_type_enum tag_type>
        requires (tag_type == tag_list)
        std::optional<list *> push(tag_type_enum tag_type_in, const std::function<void(list *)> &builder = nullptr)
        {
            auto container = mem::pmr::make_pmr_unique<list>(pmr_rsrc, this, "", tag_type_in);

            if (builder) builder(container.get());
            tags.push_back(static_cast<void *>(container.get()));

            count++;
            return container.release();
        }

        template<tag_type_enum tag_type>
        requires is_nbt_primitive<tag_type>
        void push(tag_prim_t<tag_type> value)
        {
            auto tag_ptr = mem::pmr::make_pmr_unique<primitive_tag>(pmr_rsrc, type);

            std::memcpy(static_cast<void *>(&(tag_ptr->value.generic)), static_cast<const void *>(&value), sizeof(tag_prim_t<tag_type>));
            tags.push_back(tag_ptr.get());
            count++;

            static_cast<void>(tag_ptr.release());
        }

        template<tag_type_enum tag_type>
        requires (tag_type == tag_string)
        void push(const std::string_view &str_in)
        {
            if (tag_type != this->type) throw std::runtime_error("Attempt to push value of wrong type to NBT list.");
            push_array_general<char>(str_in);
            count++;
        }

        template<tag_type_enum tag_type, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, std::size_t N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type>
        void push(const std::array<V, N> &values)
        {
            if (tag_type != this->type) throw std::runtime_error("Attempt to push value of wrong type to NBT list.");
            push_array_general<V>(values);
            count++;
        }

        template<tag_type_enum tag_type, template<class, class...> class C = std::initializer_list, class V = std::remove_pointer_t<tag_prim_t<tag_type>>, class... N>
        requires is_nbt_array<tag_type> && is_nbt_type_match<std::add_pointer_t<V>, tag_type> && util::is_simple_iterable<C<V, N...>, V>
        void push(const C<V, N...> &values)
        {
            if (tag_type != this->type) throw std::runtime_error("Attempt to push value of wrong type to NBT list.");
            push_array_general<V>(values);
            count++;
        }

        void reserve(size_t count_in)
        { tags.reserve(count_in); }

        template<tag_type_enum tag_type>
        struct range
        {
            list *list_ptr;

            auto begin()
            { return list_ptr->begin<tag_type>(); }

            auto end()
            { return list_ptr->end<tag_type>(); }
        };

        ~list();
    private:
        friend class compound;

        template<class T, class... Args>
        friend auto mem::pmr::make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
        requires (!std::is_array_v<T>);

        explicit list(std::variant<compound *, list *> parent_in, std::string_view name_in, tag_type_enum tag_type_in);
        explicit list(char **itr_in, const char *itr_end, std::variant<compound *, list *>, mem::pmr::unique_ptr<std::pmr::string> name_in, tag_type_enum tag_type_in);

        char *read(char *itr, const char *itr_end);
        void adjust_size(int64_t by);
        void remove_container(std::variant<compound *, list *> container);

        template<typename V>
        void push_array_general(const auto &values)
        {
            auto tag_ptr = mem::pmr::make_pmr_unique<primitive_tag>(pmr_rsrc, type);
            tags.push_back(tag_ptr.get()); // Placed here for exception safety

            tag_ptr->value.generic_ptr = pmr_rsrc->allocate(sizeof(V) * values.size() + padding_size, tag_properties[type].size);

            tag_ptr->resize(values.size());
            for (int idx = 0; const auto &value: values)
                static_cast<V *>(tag_ptr->value.generic_ptr)[idx++] = value;

            static_cast<void>(tag_ptr.release());
        }

        tag_list_t tags;

        uint16_t depth    = 0;
        size_t   size     = 0;
        int64_t  max_size = -1;
    };

    static_assert(std::random_access_iterator<list::iterator<tag_compound>>);
    static_assert(std::random_access_iterator<list::iterator<tag_list>>);
    static_assert(std::random_access_iterator<list::iterator<tag_int>>);
    static_assert(std::random_access_iterator<list::iterator<tag_string>>);
    static_assert(std::random_access_iterator<list::iterator<tag_int_array>>);
}

#endif //MELON_NBT_LIST_H
