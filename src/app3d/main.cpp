#include "image_loader.h"
#include "main_window.h"
#include "model_loader.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "interfaces/i_rendering_driver.h"
#include "rel/camera.h"
#include "util/range_helpers.h"

#include <uxs/db/json.h>
#include <uxs/dynarray.h>
#include <uxs/io/filebuf.h>
#include <uxs/io/iostate.h>

#include <array>
#include <chrono>
#include <exception>
#include <thread>

using namespace app3d;

class Timer {
 public:
    double getCurrent() const { return current_; }
    double getDelta() const { return delta_; }

    void update() {
        const auto time_now = is_suspended_ ? start_ : std::chrono::high_resolution_clock::now();
        const double time = last_resume_time_ + std::chrono::duration<float>(time_now - start_).count();
        delta_ = time - current_;
        current_ = time;
    }

    void suspend() {
        if (is_suspended_) { return; }
        update();
        last_resume_time_ = current_;
        is_suspended_ = true;
    }

    void resume() {
        if (!is_suspended_) { return; }
        start_ = std::chrono::high_resolution_clock::now();
        is_suspended_ = false;
    }

 private:
    std::chrono::high_resolution_clock::time_point start_{};
    bool is_suspended_ = true;
    double last_resume_time_ = 0.f;
    double current_ = 0.f;
    double delta_ = 0.f;
};

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

    void onIdle() override {
        if (is_window_minimized_) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
            frame_counter_ = 0;
            return;
        }

        const auto time_now = std::chrono::high_resolution_clock::now();
        if (frame_counter_ == 0) {
            time_fps_last_ = time_now;
        } else {
            const double delta = std::chrono::duration<double>(time_now - time_fps_last_).count();
            if (delta >= 2.) {
                logInfo("fps = {:.1f}", frame_counter_ / delta);
                frame_counter_ = 0;
                time_fps_last_ = time_now;
            }
        }

        timer_.update();
        if (!renderScene()) { terminate(-1); }
        ++frame_counter_;
    }

    void onEvent(Event event) override {
        switch (event) {
            case Event::MINIMIZE: {
                is_window_minimized_ = true;
                timer_.suspend();
            } break;
            case Event::ENTER_SIZING_OR_MOVING: {
                is_window_sizing_or_moving_ = true;
                timer_.suspend();
            } break;
            case Event::EXIT_SIZING_OR_MOVING: {
                is_window_sizing_or_moving_ = false;
                if (!needToSuspendTime()) {
                    timer_.resume();
                    frame_counter_ = 0;
                }
            } break;
            default: break;
        }
    }

    void onResize() override {
        is_window_minimized_ = false;
        if (!needToSuspendTime()) { timer_.resume(); }
        if (!recreateSwapChain()) { terminate(-1); }
    }

    void onMouseButtonEvent(KeyCode button, bool state, std::int32_t x, std::int32_t y) override {
        if (button != KeyCode::MOUSE_LBUTTON && button != KeyCode::MOUSE_MBUTTON) { return; }
        if (state) {
            rel::Vec2f p{float(x - .5f * viewport_extent_.width) / viewport_extent_.height,
                         float(.5f * viewport_extent_.height - y) / viewport_extent_.height};
            manipulator_.startDragging(p, button == KeyCode::MOUSE_LBUTTON ?
                                              rel::OrbitCameraManipulator::DragAction::ROTATE :
                                              rel::OrbitCameraManipulator::DragAction::MOVE);
        } else {
            manipulator_.stopDragging();
        }
    }

    void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        rel::Vec2f p{float(x - .5f * viewport_extent_.width) / viewport_extent_.height,
                     float(.5f * viewport_extent_.height - y) / viewport_extent_.height};
        manipulator_.drag(p);
    }

    void onMouseWheel(float distance, std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        manipulator_.moveZ(.2f * distance);
    }

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::high_resolution_clock::time_point time_fps_last_{};
    Timer timer_;
    bool is_window_minimized_ = false;
    bool is_window_sizing_or_moving_ = false;
    bool is_inverted_y_ndc_ = false;
    rel::Extent2u viewport_extent_{};

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
    rel::OrbitCameraManipulator manipulator_{camera_};

    bool needToSuspendTime() const { return is_window_minimized_ || is_window_sizing_or_moving_; }

    bool recreateSwapChain() {
        if (!swap_chain_->recreate(swap_chain_opts_)) { return false; }
        frame_counter_ = 0;
        viewport_extent_ = render_target_->getImageExtent();
        return true;
    }

    util::ref_ptr<rel::IShaderModule> compileShaderModule(const char* filename, const char* target);
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

    auto name = app_info.value<std::string>("name");

    if (!createWindow(app_info.value_or<const char*>("name", ""), 1280, 1024)) { return -1; }

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

    if (!(swap_chain_ = device_->createSwapChain(*surface_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget(JSON({"use_depth" : true})))) { return -1; }
    is_inverted_y_ndc_ = render_target_->isInvertedNdcY();
    viewport_extent_ = render_target_->getImageExtent();

    if (!initScene()) { return -1; }

    showWindow();

    timer_.resume();
    return 0;
}

util::ref_ptr<rel::IShaderModule> App3DMainWindow::compileShaderModule(const char* filename, const char* target) {
    if (uxs::filebuf ifile(filename, "r"); ifile) {
        rel::DataBlob shader_text(ifile.seek(0, uxs::seekdir::end));
        ifile.seek(0);
        shader_text.truncate(ifile.read(shader_text.getTextBuffer()));

        uxs::db::value args;
        args["filename"] = filename;
        args["target"] = target;

        rel::DataBlob compiler_output;
        auto shader_binary = driver_->compileShader(shader_text, args, compiler_output);
        if (shader_binary.isEmpty()) {
            logError("{}", compiler_output.getTextView());
            return nullptr;
        }

        if (!compiler_output.isEmpty()) { logWarning("{}", compiler_output.getTextView()); }

        return device_->createShaderModule(std::move(shader_binary));
    } else {
        logError("couldn't open '{}' shader file", filename);
    }

    return nullptr;
}

bool App3DMainWindow::initScene() {
    vertex_shader_module_ = compileShaderModule("data/shaders/transform/vert.hlsl", "vs_6_0");
    if (!vertex_shader_module_) { return false; }

    pixel_shader_module_ = compileShaderModule("data/shaders/transform/pix.hlsl", "ps_6_0");
    if (!pixel_shader_module_) { return false; }

    const auto pipeline_layout_config = JSON({
        "descriptor_set_layouts" : [ {
            "descriptor_list" : [
                {"type" : "COMBINED_TEXTURE_SAMPLER", "shader_visibility" : "PIXEL"},  //
                {"type" : "CONSTANT_BUFFER", "shader_visibility" : "VERTEX"}           //
            ]
        } ]
    });

    if (!(pipeline_layout_ = device_->createPipelineLayout(pipeline_layout_config))) { return false; }

    const auto pipeline_config = JSON({
        "dynamic_primitive_topology" : true,
        "dynamic_vertex_stride" : true,
        "stages" : [
            {"stage" : "VERTEX", "entry" : "main"},  //
            {"stage" : "PIXEL", "entry" : "main"}    //
        ],
        "vertex_layouts" : [ {
            "attributes" : [
                {"name" : "POSITION", "format" : "FLOAT3"},  //
                {"name" : "NORMAL", "format" : "FLOAT3"},    //
                {"name" : "TEXCOORD", "format" : "FLOAT2"}   //
            ]
        } ]
    });

    if (!(pipeline_ = device_->createPipeline(*render_target_, *pipeline_layout_,
                                              std::array{vertex_shader_module_.get(), pixel_shader_module_.get()},
                                              pipeline_config))) {
        return false;
    }

    if (!loadImageFromFile("data/images/sunset.jpg", image_, 4)) { return false; }

    const rel::TextureDesc texture_desc{
        .extent = {.width = image_.width, .height = image_.height, .depth = 1},
    };

    if (!(texture_ = device_->createTexture(texture_desc))) { return false; }

    if (!texture_->updateTexture(image_.data.data(), 0,
                                 std::array{rel::UpdateTextureDesc{.image_extent = texture_desc.extent}})) {
        return false;
    }

    if (!(sampler_ = device_->createSampler(rel::SamplerDesc{
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
        if (!(frame.descriptor_set = pipeline_layout_->createDescriptorSet(0))) { return false; }
        if (!(frame.cbuffer0 = device_->createBuffer(rel::BufferType::CONSTANT, sizeof(frame.cb0)))) { return false; }
        frame.descriptor_set->updateCombinedTextureSamplerDescriptor(*texture_, *sampler_, 0);
        frame.descriptor_set->updateConstantBufferDescriptor(*frame.cbuffer0, 0);
    }

    if (!loadModelFromObjFile("data/models/knot.obj",
                              LoadModelFlags::LOAD_NORMALS | LoadModelFlags::LOAD_TEXCOORDS | LoadModelFlags::UNIFY,
                              model_)) {
        return false;
    }

    if (!(vertex_buffer_ = device_->createBuffer(rel::BufferType::VERTEX, util::as_byte_span(model_.data).size()))) {
        return false;
    }

    if (!vertex_buffer_->updateBuffer(util::as_byte_span(model_.data), 0)) { return false; }

    return true;
}

void App3DMainWindow::updateMatrices(CB0& cb0) {
    const auto m = rel::Mat4f::rotate(5.f * timer_.getCurrent(), {0.f, 1.f, 0.f});
    const auto mv = m * rel::Mat4f::lookAt(camera_.eye, camera_.center, camera_.up);
    auto p = rel::Mat4f::perspective(float(viewport_extent_.width) / viewport_extent_.height, 50.0f, 0.5f, 50.0f);
    if (is_inverted_y_ndc_) { p.m[1][1] = -p.m[1][1]; }
    cb0.mv = rel::Mat3f(mv);
    cb0.mvp = mv * p;
}

bool App3DMainWindow::renderScene() {
    auto& frame = frame_data_[n_frame_];

    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f}, 1.0f, 0);
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        if (!recreateSwapChain()) { return false; }
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, model_.vertex_stride, 0);

    render_target_->bindDescriptorSet(*frame.descriptor_set, 0);

    render_target_->setPrimitiveTopology(rel::PrimitiveTopology::TRIANGLES);
    for (const auto& part : model_.parts) { render_target_->drawGeometry(part.count, 1, part.offset, 0); }

    updateMatrices(frame.cb0);
    if (!frame.cbuffer0->updateBuffer(util::as_byte_span(std::span{&frame.cb0, 1}), 0)) { return false; }

    if (!render_target_->endRenderTarget()) { return false; }

    if (++n_frame_ == frame_data_.size()) { n_frame_ = 0; }
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
