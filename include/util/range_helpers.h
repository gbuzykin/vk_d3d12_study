#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>

namespace app3d::util {

template<typename Range, typename Ty>
concept is_contiguous_range = requires(Range r) {
    { std::data(r) + std::size(r) } -> std::convertible_to<Ty*>;
};

template<typename... Ts>
class multispan {
 public:
    multispan() = default;
    template<typename... Range>
        requires(is_contiguous_range<std::remove_reference_t<Range>, Ts> && ...)
    multispan(Range&&... ranges)
        : size_(std::get<0>(std::make_tuple(std::size(ranges)...))), ptrs_(std::data(ranges)...) {
        assert((std::size(ranges) == ...));
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    template<std::size_t N>
    auto* data() const {
        return std::get<N>(ptrs_);
    }

 private:
    std::size_t size_ = 0;
    std::tuple<Ts*...> ptrs_{};
};

template<std::ranges::contiguous_range Range>
inline auto as_byte_span(Range&& r) {
    using Ty = std::remove_reference_t<decltype(*std::data(r))>;
    static_assert(std::is_trivial_v<Ty>);
    return std::span(
        reinterpret_cast<std::conditional_t<std::is_const_v<Ty>, const std::uint8_t*, std::uint8_t*>>(std::data(r)),
        std::size(r) * sizeof(Ty));
}

}  // namespace app3d::util
