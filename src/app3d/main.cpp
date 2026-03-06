#include "main_window.h"

#include "utils/dynamic_library.h"
#include "utils/logger.h"

#include <chrono>
#include <exception>

using namespace app3d;

class App3DMainWindow : public MainWindow {
 public:
    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        if (const auto result = device_->renderTestScene(*swap_chain_);
            result != rel::RenderTargetResult::SUCCESS && result != rel::RenderTargetResult::OUT_OF_DATE) {
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
        swap_chain_ = device_->createSwapChain(*surface_, swap_chain_create_info_);
        if (!swap_chain_) {
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

    rel::DesiredDeviceCaps device_caps_{};
    rel::IDevice* device_ = nullptr;

    rel::SwapChainCreateInfo swap_chain_create_info_{};
    rel::ISwapChain* swap_chain_ = nullptr;
};

int App3DMainWindow::init(int argc, char** argv) {
    void* driver_library = loadDynamicLibrary(".", "app3d-rel-vulkan");
    if (!driver_library) { return -1; }

    auto* entry = (rel::GetDriverDescriptorFuncPtr)getDynamicLibraryEntry(driver_library,
                                                                          "app3dGetRenderingDriverDescriptor");
    if (!entry) { return -1; }

    std::string app_name{"App3D"};

    driver_ = entry()->create_func();
    if (!driver_ || !driver_->init(rel::ApplicationInfo{app_name.c_str(), {1, 0, 0}})) { return -1; }

    if (!createWindow(app_name, 1280, 1024)) { return -1; }

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

    swap_chain_ = device_->createSwapChain(*surface_, swap_chain_create_info_);
    if (!swap_chain_) { return -1; }

    if (!device_->prepareTestScene(*surface_)) { return -1; }

    showWindow();
    time_fps_last_ = std::chrono::system_clock::now();
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
