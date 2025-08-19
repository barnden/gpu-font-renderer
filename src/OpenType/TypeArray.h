#pragma once

#include <type_traits>
#include <tuple>
#include <cstddef>

template<class... Ts>
struct type_array {
    using tuple_t = std::tuple<Ts...>;

    template<size_t I>
    using get = std::tuple_element_t<I, tuple_t>;

    static constexpr size_t size = sizeof...(Ts);
};
