#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"

#include <chrono>
#include <exception>

using namespace app3d;

class App3DMainWindow : public MainWindow {
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

        const auto result = device_->renderTestScene(*swap_chain_);
        if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
            scheduleRecreateSwapChain();
            if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
        } else if (result != rel::RenderTargetResult::SUCCESS) {
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

    void scheduleRecreateSwapChain() {
        recreate_swap_chain_timer_start_ = std::chrono::high_resolution_clock::now();
        recreate_swap_chain_scheduled_ = true;
    }
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
    if (!driver_ || !driver_->init(app_info)) { return -1; }

    if (!createWindow(app_info.value<std::string>("name"), 1280, 1024)) { return -1; }

    surface_ = driver_->createSurface(getWindowDescriptor());
    if (!surface_) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    device_ = driver_->createDevice(device_index, device_caps_);
    if (!device_) { return -1; }

    swap_chain_ = surface_->createSwapChain(*device_, swap_chain_opts_);
    if (!swap_chain_) { return -1; }

    if (!device_->prepareTestScene(*surface_)) { return -1; }

    showWindow();
    time_fps_last_ = std::chrono::high_resolution_clock::now();
    return 0;
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
