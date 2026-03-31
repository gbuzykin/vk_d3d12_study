#include "image_loader.h"

#include "common/logger.h"

#include <cstddef>
#include <memory>

using namespace app3d;

bool app3d::loadImageFromFile(const char* filename, Image& image, std::uint32_t num_requested_components) {
    int width = 0;
    int height = 0;
    int num_components = 0;
    std::unique_ptr<std::uint8_t, void (*)(void*)> stbi_data(
        stbi_load(filename, &width, &height, &num_components, num_requested_components), stbi_image_free);

    if (!stbi_data || width <= 0 || height <= 0 || num_components <= 0) {
        logError("error loading image '{}'", filename);
        return false;
    }

    image.width = width;
    image.height = height;
    image.num_components = num_components;
    const std::size_t data_size = std::size_t(width) * std::size_t(height) *
                                  (0 < num_requested_components ? num_requested_components : num_components);

    image.data.assign(stbi_data.get(), stbi_data.get() + data_size);
    return true;
}
