#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "interfaces/i_rendering_driver.h"

#include <exception>

using namespace app3d;

class App3DMainWindow : public MainWindow {
 public:
    int init(int argc, char** argv);

 private:
    std::unique_ptr<rel::IRenderingDriver> driver_;
    uxs::db::value device_caps_{};
    rel::IDevice* device_ = nullptr;
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

    showWindow();
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
