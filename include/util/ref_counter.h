#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>

namespace app3d::util {

class ref_counter {
 public:
    ref_counter() noexcept = default;
    virtual ~ref_counter() = default;
    ref_counter(const ref_counter&) = delete;
    ref_counter& operator=(const ref_counter&) = delete;

    void ref() noexcept { ++ref_count_; }
    void unref() noexcept {
        if (--ref_count_ == 0) { delete this; }
    }
    std::uint64_t ref_count() const noexcept { return ref_count_; }

 private:
    std::atomic<std::uint64_t> ref_count_{0};
};

template<typename Ty>
struct ref_inc;

template<typename Ty>
struct ref_dec;

template<typename Ty>
concept has_ref_method = requires(Ty p) { p.ref(); };

template<typename Ty>
concept has_unref_method = requires(Ty p) { p.unref(); };

template<has_ref_method Ty>
struct ref_inc<Ty> {
    void operator()(Ty& p) const { p.ref(); }
};

template<has_unref_method Ty>
struct ref_dec<Ty> {
    void operator()(Ty& p) const { p.unref(); }
};

template<typename Ty>
class reference {
 public:
    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    reference(Ty2& r) noexcept : p_(&r) {
        ref_inc<Ty>{}(r);
    }

    reference(const reference& r) noexcept : p_(&r) { ref_inc<Ty>{}(r.get()); }

    template<typename Ty2>
        requires std::convertible_to<Ty2*, Ty*>
    reference(const reference<Ty2>& r) noexcept : p_(&r) {
        ref_inc<Ty>{}(r.get());
    }

    ~reference() { ref_dec<Ty>{}(*p_); }

    reference& operator=(const reference&) = delete;

    Ty& get() const noexcept { return *p_; }
    operator Ty&() const noexcept { return *p_; }
    Ty* operator&() const noexcept { return p_; }

 private:
    Ty* p_ = nullptr;
};

template<typename Ty>
reference(Ty&) -> reference<Ty>;

}  // namespace app3d::util
