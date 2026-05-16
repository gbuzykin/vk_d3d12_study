#pragma once

#include "rel/config.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace app3d::rel {

class APP3D_REL_EXPORT DataBlob {
 public:
    DataBlob() noexcept = default;
    explicit DataBlob(std::size_t size)
        : size_(size),
          data_(std::make_unique<std::max_align_t[]>((size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t))) {
    }

    const std::uint8_t* getData() const noexcept { return reinterpret_cast<const std::uint8_t*>(data_.get()); }
    std::uint8_t* getData() noexcept { return reinterpret_cast<std::uint8_t*>(data_.get()); }
    std::size_t getSize() const noexcept { return size_; }
    bool isEmpty() const noexcept { return size_ == 0; }

    std::span<const std::uint8_t> getBuffer() const noexcept { return std::span{getData(), getSize()}; }
    std::span<std::uint8_t> getBuffer() noexcept { return std::span{getData(), getSize()}; }

    std::span<const char> getTextBuffer() const noexcept {
        return std::span{reinterpret_cast<const char*>(getData()), getSize()};
    }

    std::string_view getTextView() const noexcept {
        return std::string_view{reinterpret_cast<const char*>(getData()), getSize()};
    }

    std::span<char> getTextBuffer() noexcept { return std::span{reinterpret_cast<char*>(getData()), getSize()}; }

    void truncate(std::size_t size) {
        if (size < size_) { size_ = size; }
    }

 private:
    std::size_t size_ = 0;
    std::unique_ptr<std::max_align_t[]> data_;
};

}  // namespace app3d::rel
