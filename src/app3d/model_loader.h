#pragma once

#include <uxs/utility.h>

#include <cstdint>
#include <vector>

namespace app3d {

enum class LoadModelFlags {
    LOAD_NORMALS = 1,
    LOAD_TEXCOORDS = 2,
    GEN_TANGENT_SPACE_VECTORS = 4,
    UNIFY = 8,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(LoadModelFlags);

struct Model {
    struct Part {
        std::uint32_t offset;
        std::uint32_t count;
    };

    std::uint32_t vertex_stride;
    std::vector<float> data;
    std::vector<Part> parts;
};

bool loadModelFromObjFile(const char* filename, LoadModelFlags flags, Model& model);

}  // namespace app3d
