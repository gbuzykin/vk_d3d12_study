#include "model_loader.h"

#include "common/logger.h"
#include "rel/math.h"

#include <tiny_obj_loader.h>

#include <algorithm>
#include <cstddef>
#include <string>

using namespace app3d;

// Based on:
// Lengyel, Eric. "Computing Tangent Space Basis Vectors for an Arbitrary Model".
// Terathon Software 3D Graphics Library, 2001. http://www.terathon.com/code/tangent.html

void calculateTangentAndBitangent(const rel::Vec3f& normal, const rel::Vec3f& face_tangent,
                                  const rel::Vec3f& face_bitangent, rel::Vec3f& tangent, rel::Vec3f& bitangent) {
    // Gram-Schmidt orthogonalize
    tangent = normalize(face_tangent - normal * dot(normal, face_tangent));

    // Calculate handedness
    float handedness = (dot(cross(normal, tangent), face_bitangent) < 0.0f) ? -1.0f : 1.0f;

    bitangent = handedness * cross(normal, tangent);
}

void generateTangentSpaceVectors(Model& mesh) {
    struct Vertex {
        rel::Vec3f p;
        rel::Vec3f n;
        rel::Vec2f w;
        rel::Vec3f tangent;
        rel::Vec3f bitangent;
    };

    const size_t stride = sizeof(Vertex) / sizeof(float);

    for (const auto& part : mesh.parts) {
        if (part.count < 3) { continue; }
        for (std::uint32_t i = part.offset; i < part.offset + part.count - 2; i += 3) {
            Vertex& v1 = *reinterpret_cast<Vertex*>(&mesh.data[(i + 0) * stride]);
            Vertex& v2 = *reinterpret_cast<Vertex*>(&mesh.data[(i + 1) * stride]);
            Vertex& v3 = *reinterpret_cast<Vertex*>(&mesh.data[(i + 2) * stride]);

            const rel::Vec3f s1{v2.p.x - v1.p.x, v2.p.y - v1.p.y, v2.p.z - v1.p.z};
            const rel::Vec3f s2{v3.p.x - v1.p.x, v3.p.y - v1.p.y, v3.p.z - v1.p.z};

            const rel::Vec2f t1{v2.w.x - v1.w.x, v2.w.y - v1.w.y};
            const rel::Vec2f t2{v3.w.x - v1.w.x, v3.w.y - v1.w.y};

            const float r = 1.f / (t1.x * t2.y - t2.x * t1.y);
            const rel::Vec3f face_tangent = {(t2.y * s1.x - t1.y * s2.x) * r, (t2.y * s1.y - t1.y * s2.y) * r,
                                             (t2.y * s1.z - t1.y * s2.z) * r};
            const rel::Vec3f face_bitangent = {(t1.x * s2.x - t2.x * s1.x) * r, (t1.x * s2.y - t2.x * s1.y) * r,
                                               (t1.x * s2.z - t2.x * s1.z) * r};

            calculateTangentAndBitangent(v1.n, face_tangent, face_bitangent, v1.tangent, v1.bitangent);
            calculateTangentAndBitangent(v2.n, face_tangent, face_bitangent, v2.tangent, v2.bitangent);
            calculateTangentAndBitangent(v2.n, face_tangent, face_bitangent, v2.tangent, v2.bitangent);
        }
    }
}

bool app3d::loadModelFromObjFile(const char* filename, LoadModelFlags flags, Model& model) {
    tinyobj::attrib_t attribs;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attribs, &shapes, &materials, &warn, &err, filename)) {
        logError("error loading model '{}': {}", filename, err);
        return false;
    }

    // Normal vectors and texture coordinates are required to generate tangent and bitangent vectors
    if (!(flags & LoadModelFlags::LOAD_NORMALS) || !(flags & LoadModelFlags::LOAD_TEXCOORDS)) {
        flags &= ~LoadModelFlags::GEN_TANGENT_SPACE_VECTORS;
    }

    const std::uint32_t stride = 3 + (!(flags & LoadModelFlags::LOAD_NORMALS) ? 0 : 3) +
                                 (!(flags & LoadModelFlags::LOAD_TEXCOORDS) ? 0 : 2) +
                                 (!(flags & LoadModelFlags::GEN_TANGENT_SPACE_VECTORS) ? 0 : 6);

    std::uint32_t offset = 0;
    model.data.clear();
    model.parts.clear();
    model.data.reserve(stride * attribs.vertices.size());
    model.parts.reserve(shapes.size());
    for (const auto& shape : shapes) {
        const std::uint32_t part_offset = offset;

        for (const auto& index : shape.mesh.indices) {
            model.data.emplace_back(attribs.vertices[3 * index.vertex_index + 0]);
            model.data.emplace_back(attribs.vertices[3 * index.vertex_index + 1]);
            model.data.emplace_back(attribs.vertices[3 * index.vertex_index + 2]);
            ++offset;

            if (!!(flags & LoadModelFlags::LOAD_NORMALS)) {
                if (attribs.normals.size() == 0) {
                    logError("model '{}' has no normals", filename);
                    return false;
                }
                model.data.emplace_back(attribs.normals[3 * index.normal_index + 0]);
                model.data.emplace_back(attribs.normals[3 * index.normal_index + 1]);
                model.data.emplace_back(attribs.normals[3 * index.normal_index + 2]);
            }

            if (!!(flags & LoadModelFlags::LOAD_TEXCOORDS)) {
                if (attribs.texcoords.size() == 0) {
                    logError("model '{}' has no texture coordinates", filename);
                    return false;
                }
                model.data.emplace_back(attribs.texcoords[2 * index.texcoord_index + 0]);
                model.data.emplace_back(attribs.texcoords[2 * index.texcoord_index + 1]);
            }

            if (!!(flags & LoadModelFlags::GEN_TANGENT_SPACE_VECTORS)) {
                // Insert temporary tangent space vectors data
                for (int i = 0; i < 6; ++i) { model.data.emplace_back(0.0f); }
            }
        }

        const std::uint32_t part_vertex_count = offset - part_offset;
        if (part_vertex_count > 0) { model.parts.push_back(Model::Part{part_offset, part_vertex_count}); }
    }

    if (model.data.empty()) {
        logError("model '{}' is empty", filename);
        return false;
    }

    model.vertex_stride = stride * sizeof(float);

    if (!!(flags & LoadModelFlags::GEN_TANGENT_SPACE_VECTORS)) { generateTangentSpaceVectors(model); }

    if (!!(flags & LoadModelFlags::UNIFY)) {
        // Load model data and unify (normalize) its size and position
        rel::Vec3f c_min{model.data[0], model.data[1], model.data[2]};
        rel::Vec3f c_max = c_min;

        for (size_t i = 0; i < model.data.size(); i += stride) {
            c_min.x = std::min(c_min.x, model.data[i + 0]), c_max.x = std::max(c_max.x, model.data[i + 0]);
            c_min.y = std::min(c_min.y, model.data[i + 1]), c_max.y = std::max(c_max.y, model.data[i + 1]);
            c_min.z = std::min(c_min.z, model.data[i + 2]), c_max.z = std::max(c_max.z, model.data[i + 2]);
        }

        const rel::Vec3f offset{0.5f * (c_min.x + c_max.x), 0.5f * (c_min.y + c_max.y), 0.5f * (c_min.z + c_max.z)};
        const float scale = 1.f / std::max(std::max(c_max.x - offset.x, c_max.y - offset.y), c_max.z - offset.z);

        for (size_t i = 0; i < model.data.size(); i += stride) {
            model.data[i + 0] = scale * (model.data[i + 0] - offset.x);
            model.data[i + 1] = scale * (model.data[i + 1] - offset.y);
            model.data[i + 2] = scale * (model.data[i + 2] - offset.z);
        }
    }

    return true;
}
