#include "image_loader.h"
#include "main_window.h"
#include "model_loader.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "rel/camera.h"
#include "util/range_helpers.h"

#include <uxs/db/json.h>
#include <uxs/dynarray.h>
#include <uxs/io/filebuf.h>
#include <uxs/io/iostate.h>

#include <array>
#include <chrono>
#include <exception>

using namespace app3d;

struct CB0 {
    rel::Mat4f mvp;
    rel::Mat3f mv;
};

class App3DMainWindow final : public MainWindow {
 public:
    ~App3DMainWindow() {
        if (device_) { device_->waitDevice(); }
    }

    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        const auto time_now = std::chrono::high_resolution_clock::now();

        if (recreate_swap_chain_scheduled_) {
            if (std::chrono::duration<double>(time_now - recreate_swap_chain_timer_start_).count() >= 0.25) {
                if (!swap_chain_->recreateSwapChain(swap_chain_opts_)) {
                    ret_code = -1;
                    return false;
                }
                recreate_swap_chain_scheduled_ = false;
                viewport_extent_ = render_target_->getImageExtent();
            } else {
                return true;
            }
        }

        const double time = std::chrono::duration<float>(time_now - time_start_).count();
        delta_time_ = time - current_time_;
        current_time_ = time;
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
        if (button != KeyCode::MOUSE_LBUTTON && button != KeyCode::MOUSE_MBUTTON) { return; }
        if (state) {
            rel::Vec2f p{float(x - .5f * viewport_extent_.width) / viewport_extent_.height,
                         float(.5f * viewport_extent_.height - y) / viewport_extent_.height};
            manip_.startDragging(p, button == KeyCode::MOUSE_LBUTTON ? rel::OrbitCameraManipulator::DragAction::ROTATE :
                                                                       rel::OrbitCameraManipulator::DragAction::MOVE);
        } else {
            manip_.stopDragging();
        }
    }

    void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        rel::Vec2f p{float(x - .5f * viewport_extent_.width) / viewport_extent_.height,
                     float(.5f * viewport_extent_.height - y) / viewport_extent_.height};
        manip_.drag(p);
    }

    void onMouseWheel(float distance, std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        manip_.moveZ(.2f * distance);
    }

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::high_resolution_clock::time_point time_start_{};
    std::chrono::high_resolution_clock::time_point time_fps_last_{};
    std::chrono::high_resolution_clock::time_point recreate_swap_chain_timer_start_{};
    bool recreate_swap_chain_scheduled_ = false;
    rel::Extent2u viewport_extent_{};
    float current_time_ = 0;
    float delta_time_ = 0;

    util::ref_ptr<rel::IRenderingDriver> driver_;
    util::ref_ptr<rel::ISurface> surface_;

    uxs::db::value device_caps_;
    util::ref_ptr<rel::IDevice> device_;

    uxs::db::value swap_chain_opts_;
    util::ref_ptr<rel::ISwapChain> swap_chain_;

    util::ref_ptr<rel::IRenderTarget> render_target_;
    util::ref_ptr<rel::IShaderModule> vertex_shader_module_;
    util::ref_ptr<rel::IShaderModule> pixel_shader_module_;
    util::ref_ptr<rel::IPipelineLayout> pipeline_layout_;
    util::ref_ptr<rel::IPipeline> pipeline_;
    util::ref_ptr<rel::ITexture> texture_;
    util::ref_ptr<rel::ISampler> sampler_;
    util::ref_ptr<rel::IBuffer> vertex_buffer_;

    struct FrameData {
        util::ref_ptr<rel::IDescriptorSet> descriptor_set;
        util::ref_ptr<rel::IBuffer> cbuffer0;
        CB0 cb0;
    };

    std::uint32_t n_frame_ = 0;
    uxs::inline_dynarray<FrameData, 3> frame_data_;

    Image image_;
    Model model_;
    rel::Camera camera_;
    rel::OrbitCameraManipulator manip_{camera_};

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
        logInfo("device #{}: {}", device_index, driver_->getPhysicalDeviceName(device_index));
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    logInfo("selecting device #{}", device_index);

    if (!(device_ = driver_->createDevice(device_index, device_caps_))) { return -1; }

    if (!(swap_chain_ = surface_->createSwapChain(*device_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget(JSON({"use_depth" : true})))) { return -1; }
    viewport_extent_ = render_target_->getImageExtent();

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

    vertex_shader_module_ = device_->createShaderModule(vertex_shader_spv);
    if (!vertex_shader_module_) { return false; }

    pixel_shader_module_ = device_->createShaderModule(pixel_shader_spv);
    if (!pixel_shader_module_) { return false; }

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
                                              std::array{vertex_shader_module_.get(), pixel_shader_module_.get()},
                                              pipeline_config))) {
        return false;
    }

    if (!loadImageFromFile("data/images/sunset.jpg", image_, 4)) { return false; }

    const rel::TextureOpts texture_opts{
        .extent = {.width = image_.width, .height = image_.height, .depth = 1},
    };

    if (!(texture_ = device_->createTexture(texture_opts))) { return false; }

    if (!texture_->updateTexture(image_.data, {}, texture_opts.extent)) { return false; }

    if (!(sampler_ = device_->createSampler(rel::SamplerOpts{
              .filter = rel::SamplerFilter::MIN_MAG_LINEAR_MIP_POINT,
              .address_mode_u = rel::SamplerAddressMode::REPEAT,
              .address_mode_v = rel::SamplerAddressMode::REPEAT,
          }))) {
        return false;
    }

    frame_data_.resize(render_target_->getFifCount());
    if (frame_data_.empty()) {
        logError("bad render target");
        return false;
    }

    for (auto& frame : frame_data_) {
        if (!(frame.descriptor_set = device_->createDescriptorSet(*pipeline_layout_))) { return false; }
        if (!(frame.cbuffer0 = device_->createBuffer(sizeof(frame.cb0), rel::BufferType::CONSTANT))) { return false; }
        frame.descriptor_set->updateTextureSamplerDescriptor(*texture_, *sampler_, 0);
        frame.descriptor_set->updateConstantBufferDescriptor(*frame.cbuffer0, 0);
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
    const auto m = rel::Mat4f::identity();
    const auto mv = m * rel::Mat4f::lookAt(camera_.eye, camera_.center, camera_.up);
    const auto p = rel::Mat4f::perspective(float(viewport_extent_.width) / viewport_extent_.height, 50.0f, 0.5f, 50.0f);
    cb0.mv = rel::Mat3f(mv);
    cb0.mvp = mv * p;
}

bool App3DMainWindow::renderScene() {
    if (++n_frame_ == frame_data_.size()) { n_frame_ = 0; }
    auto& frame = frame_data_[n_frame_];

    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f}, 1.0f, 0);
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        scheduleRecreateSwapChain();
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->setViewport(rel::Rect{.extent = viewport_extent_}, 0.0f, 1.0f);
    render_target_->setScissor(rel::Rect{.extent = viewport_extent_});

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, 0);

    render_target_->bindDescriptorSet(*frame.descriptor_set, 0);

    render_target_->setPrimitiveTopology(rel::PrimitiveTopology::TRIANGLES);
    for (const auto& part : model_.parts) { render_target_->drawGeometry(part.count, 1, part.offset, 0); }

    updateMatrices(frame.cb0);
    if (!frame.cbuffer0->updateConstantBuffer(util::as_byte_span(std::span{&frame.cb0, 1}), 0)) { return false; }

    if (!render_target_->endRenderTarget()) { return false; }

    return true;
}

int main(int argc, char** argv) {
    try {
        App3DMainWindow win;

        setLogLevel(LogLevel::PR_DEBUG);
        int init_result = win.init(argc, argv);
        if (init_result != 0) { return init_result; }

        return win.mainLoop();

    } catch (const std::exception& e) {
        logError("exception caught: {}", e.what());
        return -1;
    }
}
