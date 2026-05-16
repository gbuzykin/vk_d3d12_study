#pragma once

#include "not_null.h"

#include <type_traits>

namespace app3d::util {

template<typename T>
struct ref_inc;

template<typename T>
struct ref_dec;

template<typename T>
class ref_ptr {
 public:
    ref_ptr() noexcept = default;
    ref_ptr(std::nullptr_t) noexcept {}

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr(U* p) noexcept : p_(p) {
        if (p) { ref_inc<T>{}(*p); }
    }

    template<typename U>
        requires std::convertible_to<U, T*>
    ref_ptr(const not_null<U>& p) noexcept(std::is_nothrow_convertible_v<U, T*>) : p_(p.get()) {
        ref_inc<T>{}(*p);
    }

    ref_ptr(const ref_ptr& p) noexcept : p_(p.get()) {
        if (p) { ref_inc<T>{}(*p); }
    }

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr(const ref_ptr<U>& p) noexcept : p_(p.get()) {
        if (p) { ref_inc<T>{}(*p); }
    }

    ref_ptr(ref_ptr&& p) noexcept : p_(p.release()) {}

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr(ref_ptr<U>&& p) noexcept : p_(p.release()) {}

    ~ref_ptr() {
        if (p_) { ref_dec<T>{}(*p_); }
    }

    explicit operator bool() const noexcept { return p_ != nullptr; }
    T* get() const noexcept { return p_; }
    T* operator->() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }

    T** reset_and_get_address() noexcept {
        ref_ptr<T>().swap(*this);
        return &p_;
    }

    T* release() noexcept {
        T* p = p_;
        p_ = nullptr;
        return p;
    }

    void reset() noexcept { ref_ptr<T>().swap(*this); }

    template<typename U>
        requires std::convertible_to<U*, T*>
    void reset(U* p) noexcept {
        ref_ptr<T>(p).swap(*this);
    }

    template<typename U>
        requires std::convertible_to<U, T*>
    void reset(const not_null<U>& p) noexcept(std::is_nothrow_convertible_v<U, T*>) {
        ref_ptr<T>(p).swap(*this);
    }

    ref_ptr& operator=(std::nullptr_t) noexcept {
        ref_ptr<T>().swap(*this);
        return *this;
    }

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr& operator=(U* p) noexcept {
        ref_ptr<T>(p).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::convertible_to<U, T*>
    ref_ptr& operator=(const not_null<U>& p) noexcept(std::is_nothrow_convertible_v<U, T*>) {
        ref_ptr<T>(p).swap(*this);
        return *this;
    }

    ref_ptr& operator=(const ref_ptr& p) noexcept {
        ref_ptr<T>(p).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr& operator=(const ref_ptr<U>& p) noexcept {
        ref_ptr<T>(p).swap(*this);
        return *this;
    }

    ref_ptr& operator=(ref_ptr&& p) noexcept {
        ref_ptr<T>(std::move(p)).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::convertible_to<U*, T*>
    ref_ptr& operator=(ref_ptr<U>&& p) noexcept {
        ref_ptr<T>(std::move(p)).swap(*this);
        return *this;
    }

    void swap(ref_ptr& p) noexcept { std::swap(p_, p.p_); }

 private:
    T* p_ = nullptr;
};

template<typename T>
ref_ptr(T p) -> ref_ptr<std::remove_cvref_t<decltype(*p)>>;

template<typename T, typename... Args>
ref_ptr<T> make_new(Args&&... args) {
    return not_null{::new T(std::forward<Args>(args)...)};
}

template<typename T>
concept has_ref_unref_methods = requires(T p) {
    p.ref();
    p.unref();
};

template<has_ref_unref_methods T>
struct ref_inc<T> {
    void operator()(T& p) const { p.ref(); }
};

template<has_ref_unref_methods T>
struct ref_dec<T> {
    void operator()(T& p) const { p.unref(); }
};

template<typename T>
concept has_get_ref_counter_method = !has_ref_unref_methods<T> && requires(T p) { p.getRefCounter(); };

template<has_get_ref_counter_method T>
struct ref_inc<T> {
    void operator()(T& p) const { p.getRefCounter().ref(); }
};

template<has_get_ref_counter_method T>
struct ref_dec<T> {
    void operator()(T& p) const { p.getRefCounter().unref(); }
};

template<typename T>
concept has_addref_release_methods = requires(T p) {
    p.AddRef();
    p.Release();
};

template<has_addref_release_methods T>
struct ref_inc<T> {
    void operator()(T& p) const { p.AddRef(); }
};

template<has_addref_release_methods T>
struct ref_dec<T> {
    void operator()(T& p) const { p.Release(); }
};

}  // namespace app3d::util

namespace std {
template<typename T>
void swap(app3d::util::ref_ptr<T>& p1, app3d::util::ref_ptr<T>& p2) noexcept {
    p1.swap(p2);
}
}  // namespace std
