#include "image_loader.h"
#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

#include <uxs/db/json.h>
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

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::high_resolution_clock::time_point time_fps_last_{};
    Timer timer_;
    bool is_window_minimized_ = false;
    bool is_window_sizing_or_moving_ = false;
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
    util::ref_ptr<rel::IDescriptorSet> descriptor_set_;
    util::ref_ptr<rel::IBuffer> vertex_buffer_;

    bool needToSuspendTime() const { return is_window_minimized_ || is_window_sizing_or_moving_; }

    bool recreateSwapChain() {
        if (!swap_chain_->recreate(swap_chain_opts_)) { return false; }
        frame_counter_ = 0;
        viewport_extent_ = render_target_->getImageExtent();
        return true;
    }

    bool initScene();
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

    if (!(swap_chain_ = device_->createSwapChain(*surface_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget(JSON({"use_depth" : true})))) { return -1; }
    viewport_extent_ = render_target_->getImageExtent();

    if (!initScene()) { return -1; }

    showWindow();

    timer_.resume();
    return 0;
}

bool App3DMainWindow::initScene() {
    std::vector<std::uint32_t> vertex_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/sampler/vert.spv", "r"); ifile) {
        vertex_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(vertex_shader_spv));
    }

    std::vector<std::uint32_t> pixel_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/sampler/pix.spv", "r"); ifile) {
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
            "descriptor_list" : [
                {"type" : "COMBINED_TEXTURE_SAMPLER", "shader_visibility" : "PIXEL"},  //
                {"type" : "CONSTANT_BUFFER", "shader_visibility" : "VERTEX"}           //
            ]
        } ]
    });

    if (!(pipeline_layout_ = device_->createPipelineLayout(pipeline_layout_config))) { return false; }

    const auto pipeline_config = JSON({
        "stages" : [
            {"stage" : "VERTEX", "entry" : "main"},  //
            {"stage" : "PIXEL", "entry" : "main"}    //
        ],
        "vertex_layouts" : [ {
            "attributes" : [
                {"name" : "POSITION", "format" : "FLOAT3"},  //
                {"name" : "TEXCOORD", "format" : "FLOAT2"}   //
            ]
        } ]
    });

    if (!(pipeline_ = device_->createPipeline(*render_target_, *pipeline_layout_,
                                              std::array{vertex_shader_module_.get(), pixel_shader_module_.get()},
                                              pipeline_config))) {
        return false;
    }

    Image image;
    if (!loadImageFromFile("data/images/sunset.jpg", image, 4)) { return false; }

    rel::Extent3u image_extent{.width = image.width, .height = image.height, .depth = 1};

    if (!(texture_ = device_->createTexture(image_extent))) { return false; }

    if (!texture_->updateTexture(image.data, {}, image_extent)) { return false; }

    if (!(sampler_ = device_->createSampler())) { return false; }

    if (!(descriptor_set_ = pipeline_layout_->createDescriptorSet(0))) { return false; }
    descriptor_set_->updateCombinedTextureSamplerDescriptor(*texture_, *sampler_, 0);

    const std::vector<float> vertices{
        -0.75f, -0.75f, 0.0f, 0.0f, 0.0f, -0.75f, 0.75f, 0.0f, 0.0f, 1.0f,
        0.75f,  -0.75f, 0.0f, 1.0f, 0.0f, 0.75f,  0.75f, 0.0f, 1.0f, 1.0f,
    };

    if (!(vertex_buffer_ = device_->createBuffer(rel::BufferType::VERTEX, sizeof(vertices[0]) * vertices.size()))) {
        return false;
    }

    if (!vertex_buffer_->updateBuffer(util::as_byte_span(vertices), 0)) { return false; }

    return true;
}

bool App3DMainWindow::renderScene() {
    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f}, 1.0f, 0);
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        if (!recreateSwapChain()) { return false; }
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->bindDescriptorSet(*descriptor_set_, 0);

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, 0);

    render_target_->setPrimitiveTopology(rel::PrimitiveTopology::TRIANGLE_STRIP);

    render_target_->drawGeometry(4, 1, 0, 0);

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
