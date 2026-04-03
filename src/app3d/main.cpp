#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "rel/math.h"
#include "util/range_helpers.h"

#include <uxs/db/json.h>
#include <uxs/dynarray.h>
#include <uxs/io/filebuf.h>
#include <uxs/io/iostate.h>

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <array>
#include <chrono>
#include <exception>
#include <vector>

using namespace app3d;

struct CB0 {
    rel::Mat4f mvp;
    rel::Mat3f mv;
};

namespace {

struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t num_components = 0;
    std::vector<std::uint8_t> data;
};

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

bool loadImageFromFile(const char* filename, Image& image, std::uint32_t num_requested_components = 0) {
    int width = 0;
    int height = 0;
    int num_components = 0;
    std::unique_ptr<unsigned char, void (*)(void*)> stbi_data(
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

bool loadModelFromObjFile(const char* filename, LoadModelFlags flags, Model& model) {
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

}  // namespace

class App3DMainWindow final : public MainWindow {
 public:
    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        const auto time_now = std::chrono::high_resolution_clock::now();
        if (recreate_swap_chain_scheduled_) {
            if (std::chrono::duration<double>(time_now - recreate_swap_chain_timer_start_).count() >= 0.25) {
                if (!surface_->createSwapChain(*device_, swap_chain_opts_)) {
                    ret_code = -1;
                    return false;
                }
                recreate_swap_chain_scheduled_ = false;
            } else {
                return true;
            }
        }

        current_time_ = std::chrono::duration<float>(time_now - time_start_).count();
        if (!renderScene()) {
            ret_code = -1;
            return false;
        }

        ++frame_counter_;
        const double delta = std::chrono::duration<double>(time_now - time_fps_last_).count();
        if (delta >= 2) {
            logInfo("fps = {:.1f}", frame_counter_ / delta);
            frame_counter_ = 0;
            time_fps_last_ = time_now;
        }
        return true;
    }

    bool onResize(int& ret_code) override {
        scheduleRecreateSwapChain();
        return true;
    }

    void onMouseButtonEvent(KeyCode button, bool state, std::int32_t x, std::int32_t y) override {
        if (button != KeyCode::MOUSE_LBUTTON) { return; }
        is_rotating_ = state;
        mouse_x0_ = x, mouse_y0_ = y;
        mouse_x_ = x, mouse_y_ = y;
        if (!state) { model_rotation0_ = model_rotation0_ * model_rotation_; }
    }

    void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        if (!is_rotating_) { return; }
        mouse_x_ = x, mouse_y_ = y;
    }

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::high_resolution_clock::time_point time_start_{};
    std::chrono::high_resolution_clock::time_point time_fps_last_{};
    std::chrono::high_resolution_clock::time_point recreate_swap_chain_timer_start_{};
    bool recreate_swap_chain_scheduled_ = false;

    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::ISurface* surface_ = nullptr;

    uxs::db::value device_caps_;
    rel::IDevice* device_ = nullptr;

    uxs::db::value swap_chain_opts_;
    rel::ISwapChain* swap_chain_ = nullptr;

    rel::IRenderTarget* render_target_ = nullptr;
    rel::IPipelineLayout* pipeline_layout_ = nullptr;
    rel::IPipeline* pipeline_ = nullptr;
    rel::ITexture* texture_ = nullptr;
    rel::ISampler* sampler_ = nullptr;
    rel::IDescriptorSet* descriptor_set_ = nullptr;
    rel::IBuffer* vertex_buffer_ = nullptr;

    Image image_;
    Model model_;

    float current_time_ = 0;
    bool is_rotating_ = false;
    std::int32_t mouse_x0_ = 0;
    std::int32_t mouse_x_ = 0;
    std::int32_t mouse_y0_ = 0;
    std::int32_t mouse_y_ = 0;
    rel::Vec3f model_center_{0.f, 0., -4.f};
    rel::Mat4f model_rotation0_{rel::Mat4f::identity()};
    rel::Mat4f model_rotation_{rel::Mat4f::identity()};

    struct FrameData {
        rel::IBuffer* cbuffer0_ = nullptr;
        CB0 cb0_;
    };

    std::uint32_t n_frame_ = 0;
    uxs::inline_dynarray<FrameData, 3> frame_data_;

    void scheduleRecreateSwapChain() {
        recreate_swap_chain_timer_start_ = std::chrono::high_resolution_clock::now();
        recreate_swap_chain_scheduled_ = true;
    }

    bool initScene();
    void updateMatrices(CB0& cb0);
    bool renderScene();
};

#define JSON(...) uxs::db::json::read_from_string(#__VA_ARGS__)

int App3DMainWindow::init(int argc, char** argv) {
    void* driver_library = loadDynamicLibrary(".", "app3d-rel-vulkan");
    if (!driver_library) { return -1; }

    auto* entry = (rel::GetDriverDescriptorFuncPtr)getDynamicLibraryEntry(driver_library,
                                                                          "app3dGetRenderingDriverDescriptor");
    if (!entry) { return -1; }

    const auto app_info = JSON({"name" : "App3D", "version" : [ 1, 0, 0 ]});

    if (!(driver_ = entry()->create_func()) || !driver_->init(app_info)) { return -1; }

    if (!createWindow(app_info.value<std::string>("name"), 1280, 1024)) { return -1; }

    if (!(surface_ = driver_->createSurface(getWindowDescriptor()))) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();

    device_caps_ = JSON({"needs_compute" : true});

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    if (!(device_ = driver_->createDevice(device_index, device_caps_))) { return -1; }

    if (!(swap_chain_ = surface_->createSwapChain(*device_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget(JSON({"use_depth" : true})))) { return -1; }

    if (!initScene()) { return -1; }

    showWindow();
    time_start_ = std::chrono::high_resolution_clock::now();
    time_fps_last_ = time_start_;
    return 0;
}

bool App3DMainWindow::initScene() {
    std::vector<std::uint32_t> vertex_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/transform/vert.spv", "r"); ifile) {
        vertex_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(vertex_shader_spv));
    }

    std::vector<std::uint32_t> pixel_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/transform/pix.spv", "r"); ifile) {
        pixel_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(pixel_shader_spv));
    }

    auto* vertex_shader_module = device_->createShaderModule(vertex_shader_spv);
    if (!vertex_shader_module) { return false; }

    auto* pixel_shader_module = device_->createShaderModule(pixel_shader_spv);
    if (!pixel_shader_module) { return false; }

    const auto pipeline_layout_config = JSON({
        "descriptor_set_layouts" : [ {
            "desc_list" : [
                {"binding" : 0, "type" : "texture_sampler", "count" : 1, "stages" : ["pixel"]},
                {"binding" : 1, "type" : "constant_buffer", "count" : 1, "stages" : ["vertex"]}
            ]
        } ]
    });

    if (!(pipeline_layout_ = device_->createPipelineLayout(pipeline_layout_config))) { return false; }

    const auto pipeline_config = JSON({
        "stages" : [
            {"stage" : "vertex", "module_index" : 0, "entry" : "main"},
            {"stage" : "pixel", "module_index" : 1, "entry" : "main"}
        ],
        "vertex_layouts" : [ {
            "binding" : 0,
            "stride" : 32,
            "attributes" : {
                "0" : {"format" : "float3", "offset" : 0},
                "1" : {"format" : "float3", "offset" : 12},
                "2" : {"format" : "float2", "offset" : 24}
            }
        } ]
    });

    if (!(pipeline_ = device_->createPipeline(*render_target_, *pipeline_layout_,
                                              std::array{vertex_shader_module, pixel_shader_module}, pipeline_config))) {
        return false;
    }

    if (!loadImageFromFile("data/images/sunset.jpg", image_, 4)) { return false; }

    const rel::Extent3u image_extent{.width = image_.width, .height = image_.height, .depth = 1};

    if (!(texture_ = device_->createTexture(image_extent))) { return false; }

    if (!texture_->updateTexture(image_.data, {}, image_extent)) { return false; }

    if (!(descriptor_set_ = device_->createDescriptorSet(*pipeline_layout_))) { return false; }

    if (!(sampler_ = device_->createSampler())) { return false; }
    descriptor_set_->updateTextureSamplerDescriptor(*texture_, *sampler_, 0);

    frame_data_.resize(render_target_->getFrameInFlightCount());
    for (auto& frame : frame_data_) {
        if (!(frame.cbuffer0_ = device_->createBuffer(sizeof(frame.cb0_), rel::BufferType::CONSTANT))) { return false; }
    }

    if (!loadModelFromObjFile("data/models/knot.obj",
                              LoadModelFlags::LOAD_NORMALS | LoadModelFlags::LOAD_TEXCOORDS | LoadModelFlags::UNIFY,
                              model_)) {
        return false;
    }

    if (!(vertex_buffer_ = device_->createBuffer(util::as_byte_span(model_.data).size(), rel::BufferType::VERTEX))) {
        return false;
    }

    if (!vertex_buffer_->updateVertexBuffer(util::as_byte_span(model_.data), 0)) { return false; }

    return true;
}

void App3DMainWindow::updateMatrices(CB0& cb0) {
    const auto extent = swap_chain_->getImageExtent();

    model_rotation_ = rel::Mat4f::identity();
    if (is_rotating_) {
        const rel::Vec3f v1{float(mouse_x0_ - .5f * extent.width) / extent.width,
                            float(.5f * extent.height - mouse_y0_) / extent.height, 1.f};
        const rel::Vec3f v2{float(mouse_x_ - .5f * extent.width) / extent.width,
                            float(.5f * extent.height - mouse_y_) / extent.height, 1.f};
        const rel::Vec3f rot_axis = rel::cross(rel::normalize(v1), rel::normalize(v2));

        const float sensitivity = 200.f;
        const float a = rel::length(rot_axis);
        if (a >= 0.001) { model_rotation_ = rel::Mat4f::rotate(sensitivity * std::asin(a), (1.f / a) * rot_axis); }
    }

    const auto mv = model_rotation0_ * model_rotation_ * rel::Mat4f::translate(model_center_);

    cb0.mv = rel::Mat3f(mv);

    cb0.mvp = mv * rel::Mat4f::perspective(float(extent.width) / float(extent.height), 50.0f, 0.5f, 10.0f);
}

bool App3DMainWindow::renderScene() {
    auto& frame = frame_data_[n_frame_++];
    if (n_frame_ == frame_data_.size()) { n_frame_ = 0; }

    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f}, 1.0f, 0);
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        scheduleRecreateSwapChain();
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    const auto extent = swap_chain_->getImageExtent();
    render_target_->setViewport(rel::Rect{.extent = extent}, 0.0f, 1.0f);
    render_target_->setScissor(rel::Rect{.extent = extent});

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, 0);
    render_target_->bindDescriptorSet(*descriptor_set_, 0);
    descriptor_set_->updateConstantBufferDescriptor(*frame.cbuffer0_, 0);

    render_target_->setPrimitiveTopology(rel::PrimitiveTopology::TRIANGLES);
    for (const auto& part : model_.parts) { render_target_->drawGeometry(part.count, 1, part.offset, 0); }

    updateMatrices(frame.cb0_);
    if (!frame.cbuffer0_->updateConstantBuffer(util::as_byte_span(std::span{&frame.cb0_, 1}), 0)) { return false; }

    if (!render_target_->endRenderTarget()) { return false; }

    return true;
}

int main(int argc, char** argv) {
    try {
        App3DMainWindow win;

        int init_result = win.init(argc, argv);
        if (init_result != 0) { return init_result; }

        return win.mainLoop();

    } catch (const std::exception& e) {
        logError("exception caught: {}", e.what());
        return -1;
    }
}
