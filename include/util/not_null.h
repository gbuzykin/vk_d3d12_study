#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

namespace app3d::util {

template<typename T>
class not_null {
 public:
    template<typename U>
        requires std::constructible_from<T, U&&>
    constexpr not_null(U&& u) noexcept(std::is_nothrow_constructible_v<T, U&&>) : p_(std::forward<U>(u)) {}

    template<typename U>
        requires std::constructible_from<T, const U&>
    constexpr not_null(const not_null<U>& p) noexcept(std::is_nothrow_constructible_v<T, const U&>) : p_(p.get()) {}

    template<typename U>
        requires std::constructible_from<T, U&&>
    constexpr not_null(not_null<U>&& p) noexcept(std::is_nothrow_constructible_v<T, U&&>) : p_(std::move(p).get()) {}

    constexpr const T& get() const& noexcept { return p_; }
    constexpr T&& get() && noexcept { return std::move(p_); }
    constexpr operator const T&() const noexcept { return p_; }
    constexpr const T& operator->() const noexcept { return p_; }
    constexpr decltype(auto) operator*() const noexcept(noexcept(*std::declval<T>())) { return *p_; }

    constexpr not_null(const not_null&) noexcept(std::is_nothrow_copy_constructible_v<T>) = default;
    constexpr not_null(not_null&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;

    constexpr not_null& operator=(const not_null&) noexcept(std::is_nothrow_copy_assignable_v<T>) = default;
    constexpr not_null& operator=(not_null&&) noexcept(std::is_nothrow_move_assignable_v<T>) = default;

    not_null(std::nullptr_t) = delete;
    not_null& operator=(std::nullptr_t) = delete;

    void swap(not_null& p) noexcept(std::is_nothrow_swappable_v<T>) { std::swap(p_, p.p_); }

 private:
    T p_;
};

template<typename T>
not_null(T) -> not_null<T>;

template<typename T, typename U>
constexpr auto operator==(const not_null<T>& lhs, const not_null<U>& rhs) noexcept(noexcept(lhs.get() == rhs.get()))
    -> decltype(lhs.get() == rhs.get()) {
    return lhs.get() == rhs.get();
}

template<typename T, typename U>
constexpr auto operator<=>(const not_null<T>& lhs, const not_null<U>& rhs) noexcept(noexcept(lhs.get() <=> rhs.get()))
    -> decltype(lhs.get() <=> rhs.get()) {
    return lhs.get() <=> rhs.get();
}

}  // namespace app3d::util

namespace std {
template<typename T>
void swap(app3d::util::not_null<T>& p1, app3d::util::not_null<T>& p2) noexcept(std::is_nothrow_swappable_v<T>) {
    p1.swap(p2);
}
}  // namespace std
