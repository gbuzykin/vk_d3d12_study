#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "util/range_helpers.h"

#include <uxs/io/filebuf.h>

#include <chrono>
#include <exception>

using namespace app3d;

class App3DMainWindow final : public MainWindow {
 public:
    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        if (recreate_swap_chain_scheduled_) {
            const auto timer_now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double>(timer_now - recreate_swap_chain_timer_start_).count() > 0.25) {
                if (!surface_->createSwapChain(*device_, swap_chain_opts_)) {
                    ret_code = -1;
                    return false;
                }
                recreate_swap_chain_scheduled_ = false;
            } else {
                return true;
            }
        }

        if (!renderScene()) {
            ret_code = -1;
            return false;
        }

        const auto time_now = std::chrono::high_resolution_clock::now();
        ++frame_counter_;
        const double delta = std::chrono::duration<double>(time_now - time_fps_last_).count();
        if (delta > 2) {
            logInfo("fps = {}", frame_counter_ / delta);
            frame_counter_ = 0;
            time_fps_last_ = time_now;
        }
        return true;
    }

    bool onResize(int& ret_code) override {
        scheduleRecreateSwapChain();
        return true;
    }

 private:
    std::uint64_t frame_counter_ = 0;
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
    rel::IPipeline* pipeline_ = nullptr;
    rel::IBuffer* vertex_buffer_ = nullptr;

    void scheduleRecreateSwapChain() {
        recreate_swap_chain_timer_start_ = std::chrono::high_resolution_clock::now();
        recreate_swap_chain_scheduled_ = true;
    }

    bool initScene();
    bool renderScene();
};

int App3DMainWindow::init(int argc, char** argv) {
    void* driver_library = loadDynamicLibrary(".", "app3d-rel-vulkan");
    if (!driver_library) { return -1; }

    auto* entry = (rel::GetDriverDescriptorFuncPtr)getDynamicLibraryEntry(driver_library,
                                                                          "app3dGetRenderingDriverDescriptor");
    if (!entry) { return -1; }

    const uxs::db::value app_info{
        {"name", "App3D"},
        {"version", {1, 0, 0}},
    };

    if (!(driver_ = entry()->create_func()) || !driver_->init(app_info)) { return -1; }

    if (!createWindow(app_info.value<std::string>("name"), 1280, 1024)) { return -1; }

    if (!(surface_ = driver_->createSurface(getWindowDescriptor()))) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    if (!(device_ = driver_->createDevice(device_index, device_caps_))) { return -1; }

    if (!(swap_chain_ = surface_->createSwapChain(*device_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget({}))) { return -1; }

    if (!initScene()) { return -1; }

    showWindow();
    time_fps_last_ = std::chrono::high_resolution_clock::now();
    return 0;
}

bool App3DMainWindow::initScene() {
    std::vector<std::uint32_t> vertex_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/simple/vert.spv", "r"); ifile) {
        vertex_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(vertex_shader_spv));
    }

    std::vector<std::uint32_t> pixel_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/simple/pix.spv", "r"); ifile) {
        pixel_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(pixel_shader_spv));
    }

    auto* vertex_shader_module = device_->createShaderModule(vertex_shader_spv);
    if (!vertex_shader_module) { return false; }

    auto* pixel_shader_module = device_->createShaderModule(pixel_shader_spv);
    if (!pixel_shader_module) { return false; }

    if (!(pipeline_ = device_->createPipeline(*render_target_, std::array{vertex_shader_module, pixel_shader_module},
                                              {}))) {
        return false;
    }

    const std::vector<float> vertices{0.0f, -0.75f, 0.0f, -0.75f, 0.75f, 0.0f, 0.75f, 0.75f, 0.0f};

    if (!(vertex_buffer_ = device_->createBuffer(sizeof(vertices[0]) * vertices.size()))) { return -1; }

    if (!vertex_buffer_->updateBuffer(util::as_byte_span(vertices), 0)) { return -1; }

    return true;
}

bool App3DMainWindow::renderScene() {
    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f});
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        scheduleRecreateSwapChain();
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->setViewport(rel::Rect{.extent = swap_chain_->getImageExtent()}, 0.0f, 1.0f);

    render_target_->setScissor(rel::Rect{.extent = swap_chain_->getImageExtent()});

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, 0);

    render_target_->drawGeometry(3, 1, 0, 0);

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
