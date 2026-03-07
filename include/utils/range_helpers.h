#pragma once

#include <cassert>
#include <concepts>
#include <span>
#include <tuple>

namespace app3d {

template<typename Range, typename Ty>
concept IsContiguousRange = requires(Range r) {
    { std::data(r) + std::size(r) } -> std::convertible_to<Ty*>;
};

template<typename... Ts>
class MultiSpan {
 public:
    MultiSpan() = default;
    template<typename... Range>
        requires(IsContiguousRange<std::remove_reference_t<Range>, Ts> && ...)
    MultiSpan(Range&&... ranges)
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

}  // namespace app3d
