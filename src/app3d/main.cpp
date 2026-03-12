#include "main_window.h"

#include "utils/dynamic_library.h"
#include "utils/logger.h"

#include <uxs/io/filebuf.h>

#include <chrono>
#include <exception>

using namespace app3d;

class App3DMainWindow final : public MainWindow {
 public:
    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        if (!renderScene()) {
            ret_code = -1;
            return false;
        }

        const auto time_now = std::chrono::system_clock::now();
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
        if (!surface_->createSwapChain(*device_)) {
            ret_code = -1;
            return false;
        }
        return true;
    }

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::system_clock::time_point time_fps_last_{};

    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::ISurface* surface_ = nullptr;

    uxs::db::value device_caps_;
    rel::IDevice* device_ = nullptr;
    rel::ISwapChain* swap_chain_ = nullptr;
    rel::IRenderTarget* render_target_ = nullptr;
    rel::IShaderModule* vertex_shader_module_ = nullptr;
    rel::IShaderModule* fragment_shader_module_ = nullptr;
    rel::IPipeline* pipeline_ = nullptr;
    rel::IBuffer* vertex_buffer_ = nullptr;

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

    driver_ = entry()->create_func();
    if (!driver_ || driver_->init(app_info) != rel::ResultCode::SUCCESS) { return -1; }

    if (!createWindow(app_info.value<std::string>("name"), 1280, 1024)) { return -1; }

    surface_ = driver_->createSurface(getWindowDescriptor());
    if (!surface_) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();

    device_caps_ = {
        {"needs_compute", true},
    };

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    device_ = driver_->createDevice(device_index, device_caps_);
    if (!device_) { return -1; }

    swap_chain_ = surface_->createSwapChain(*device_);
    if (!swap_chain_) { return -1; }

    render_target_ = swap_chain_->createRenderTarget();
    if (!render_target_) { return -1; }

    if (!initScene()) { return -1; }

    showWindow();
    time_fps_last_ = std::chrono::system_clock::now();
    return 0;
}

bool App3DMainWindow::initScene() {
    std::vector<std::uint8_t> vertex_shader_spirv;
    if (uxs::bfilebuf ifile("shaders/simple/shader.vert.spv", "r"); ifile) {
        vertex_shader_spirv.resize(ifile.seek(0, uxs::seekdir::end));
        ifile.seek(0);
        vertex_shader_spirv.resize(ifile.read(vertex_shader_spirv));
    }

    std::vector<std::uint8_t> fragment_shader_spirv;
    if (uxs::bfilebuf ifile("shaders/simple/shader.frag.spv", "r"); ifile) {
        fragment_shader_spirv.resize(ifile.seek(0, uxs::seekdir::end));
        ifile.seek(0);
        fragment_shader_spirv.resize(ifile.read(fragment_shader_spirv));
    }

    vertex_shader_module_ = device_->createShaderModule(vertex_shader_spirv);
    if (!vertex_shader_module_) { return -1; }

    fragment_shader_module_ = device_->createShaderModule(fragment_shader_spirv);
    if (!fragment_shader_module_) { return -1; }

    pipeline_ = device_->createPipeline(*render_target_, std::array{vertex_shader_module_, fragment_shader_module_});
    if (!pipeline_) { return -1; }

    const std::vector<float> vertices{0.0f, -0.75f, 0.0f, -0.75f, 0.75f, 0.0f, 0.75f, 0.75f, 0.0f};

    vertex_buffer_ = device_->createBuffer(sizeof(vertices[0]) * vertices.size());
    if (!vertex_buffer_) { return -1; }

    if (vertex_buffer_->updateBuffer(sizeof(vertices[0]) * vertices.size(), vertices.data(), 0) !=
        rel::ResultCode::SUCCESS) {
        return false;
    }

    return true;
}

bool App3DMainWindow::renderScene() {
    if (const auto result = render_target_->beginRenderTarget(rel::Vec4f{0.1f, 0.2f, 0.3f, 1.0f});
        result != rel::ResultCode::SUCCESS && result != rel::ResultCode::OUT_OF_DATE) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->setViewport(rel::Rect{.extent = swap_chain_->getImageExtent()}, 0.0f, 1.0f);

    render_target_->setScissor(rel::Rect{.extent = swap_chain_->getImageExtent()});

    render_target_->bindVertexBuffer(0, *vertex_buffer_, 0);

    render_target_->drawGeometry(3, 1, 0, 0);

    if (const auto result = render_target_->endRenderTarget();
        result != rel::ResultCode::SUCCESS && result != rel::ResultCode::OUT_OF_DATE) {
        return false;
    }

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
