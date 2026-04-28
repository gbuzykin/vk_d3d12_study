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

}  // namespace app3d::util
