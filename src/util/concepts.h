//
// Created by MrGrim on 9/11/2022.
//

#ifndef MELON_UTIL_CONCEPTS_H
#define MELON_UTIL_CONCEPTS_H

#include <iterator>
#include <concepts>
#include <type_traits>

template<typename C, typename V>
concept is_simple_iterable = requires(C container) {
    requires std::same_as<std::remove_cvref_t<decltype(*(container.begin()))>, V>;
    { container.begin() } -> std::input_iterator;
    { container.end() } -> std::sentinel_for<decltype(container.begin())>;
    { container.size() } -> std::convertible_to<uint32_t>;
};

#endif //MELON_UTIL_CONCEPTS_H
