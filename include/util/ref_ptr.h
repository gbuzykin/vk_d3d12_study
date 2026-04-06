#pragma once

#include "ref_counter.h"

#include <cstddef>

namespace app3d::util {

template<typename Ty>
class ref_ptr {
 public:
    ref_ptr() noexcept = default;
    ref_ptr(std::nullptr_t) noexcept {}

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr(Ty2* p) noexcept : p_(p) {
        if (p) { ref_inc<Ty>{}(*p); }
    }

    ref_ptr(const ref_ptr& p) noexcept : p_(p.get()) {
        if (p) { ref_inc<Ty>{}(*p); }
    }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr(const ref_ptr<Ty2>& p) noexcept : p_(p.get()) {
        if (p) { ref_inc<Ty>{}(*p); }
    }

    ref_ptr(ref_ptr&& p) noexcept : p_(p.release()) {}

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr(ref_ptr<Ty2>&& p) noexcept : p_(p.release()) {}

    ~ref_ptr() {
        if (p_) { ref_dec<Ty>{}(*p_); }
    }

    explicit operator bool() const noexcept { return p_ != nullptr; }
    Ty* get() const noexcept { return p_; }
    Ty* operator->() const noexcept { return p_; }
    Ty& operator*() const noexcept { return *p_; }

    Ty* release() noexcept {
        Ty* p = p_;
        p_ = nullptr;
        return p;
    }

    void reset() noexcept {
        if (p_) { ref_dec<Ty>{}(*p_); }
        p_ = nullptr;
    }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    void reset(Ty2* p) noexcept {
        if (p) { ref_inc<Ty>{}(*p); }
        if (p_) { ref_dec<Ty>{}(*p_); }
        p_ = p;
    }

    ref_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr& operator=(Ty2* p) noexcept {
        reset(p);
        return *this;
    }

    ref_ptr& operator=(const ref_ptr& p) noexcept {
        if (this == &p) { return *this; }
        reset(p.get());
        return *this;
    }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr& operator=(const ref_ptr<Ty2>& p) noexcept {
        if (this == &p) { return *this; }
        reset(p.get());
        return *this;
    }

    ref_ptr& operator=(ref_ptr&& p) noexcept {
        if (this == &p) { return *this; }
        if (p_) { ref_dec<Ty>{}(*p_); }
        p_ = p.release();
        return *this;
    }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    ref_ptr& operator=(ref_ptr<Ty2>&& p) noexcept {
        if (this == &p) { return *this; }
        if (p_) { ref_dec<Ty>{}(*p_); }
        p_ = p.release();
        return *this;
    }

 private:
    Ty* p_ = nullptr;
};

template<typename Ty>
ref_ptr(Ty*) -> ref_ptr<Ty>;

}  // namespace app3d::util
