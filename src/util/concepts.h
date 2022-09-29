//
// Created by MrGrim on 9/11/2022.
//

#ifndef MELON_UTIL_CONCEPTS_H
#define MELON_UTIL_CONCEPTS_H

#include <iterator>
#include <concepts>
#include <type_traits>

namespace melon::util
{
    template<typename C, typename V>
    concept is_simple_iterable =
    requires(C container) {
        requires std::same_as<std::remove_cvref_t<decltype(*(container.begin()))>, V>;
        { container.begin() } -> std::input_iterator;
        { container.end() } -> std::sentinel_for<decltype(container.begin())>;
        { container.size() } -> std::convertible_to<std::size_t>;
    };

    template<class T>
    concept fundamental = std::is_fundamental_v<T>;

    template<class C>
    concept is_contiguous_owner =
    requires(C view_container, typename C::iterator itr)
    {
        view_container.data();
        view_container.size();
        view_container.erase(itr);
        view_container.clear();
    };

    template<class C>
    concept is_contiguous_view =
    requires(C view_container)
    {
        view_container.data();
        view_container.size();
        !requires(C non_view_container, typename C::iterator itr)
        {
            non_view_container.erase(itr);
            non_view_container.clear();
        };
    };
}

#endif //MELON_UTIL_CONCEPTS_H
