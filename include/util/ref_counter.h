#pragma once

#include <atomic>
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

template<typename T>
struct ref_inc;

template<typename T>
struct ref_dec;

template<typename T>
concept has_ref_method = requires(T p) { p.ref(); };

template<typename T>
concept has_unref_method = requires(T p) { p.unref(); };

template<has_ref_method T>
struct ref_inc<T> {
    void operator()(T& p) const { p.ref(); }
};

template<has_unref_method T>
struct ref_dec<T> {
    void operator()(T& p) const { p.unref(); }
};

}  // namespace app3d::util
