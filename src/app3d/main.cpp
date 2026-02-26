#include "main_window.h"
#include "rendering_iface.h"

#include "utils/dynamic_library.h"
#include "utils/print.h"

#include <exception>

using namespace app3d;

class App3DMainWindow : public MainWindow {
 public:
    int init(int argc, char** argv);

 private:
    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::IDevice* device_ = nullptr;
};

int App3DMainWindow::init(int argc, char** argv) {
    void* driver_library = loadDynamicLibrary(".", "app3d-vulkan");
    if (!driver_library) { return -1; }

    auto* entry = (rel::GetDriverDescriptorFuncPtr)getDynamicLibraryEntry(driver_library,
                                                                          "app3dGetRenderingDriverDescriptor");
    if (!entry) { return -1; }

    std::string app_name{"App3D"};

    driver_ = entry()->create_func();
    if (!driver_ || !driver_->init(rel::ApplicationInfo{app_name.c_str(), {1, 0, 0}})) { return -1; }

    if (!createWindow(app_name, 1024, 768)) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();
    rel::DesiredDeviceCaps caps{};

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, caps)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical Vulkan device");
        return -1;
    }

    device_ = driver_->createDevice(device_index, caps);
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
