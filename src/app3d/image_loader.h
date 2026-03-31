#pragma once

#include <stb_image.h>

#include <cstdint>
#include <vector>

namespace app3d {

struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t num_components = 0;
    std::vector<std::uint8_t> data;
};

bool loadImageFromFile(const char* filename, Image& image, std::uint32_t num_requested_components = 0);

}  // namespace app3d
