#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "interfaces/i_rendering_driver.h"

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

    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::ISurface* surface_ = nullptr;

    uxs::db::value device_caps_;
    rel::IDevice* device_ = nullptr;

    uxs::db::value swap_chain_opts_;
    rel::ISwapChain* swap_chain_ = nullptr;

    bool needToSuspendTime() const { return is_window_minimized_ || is_window_sizing_or_moving_; }

    bool recreateSwapChain() {
        if (!swap_chain_->recreate(swap_chain_opts_)) { return false; }
        frame_counter_ = 0;
        return true;
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

    swap_chain_ = device_->createSwapChain(*surface_, swap_chain_opts_);
    if (!swap_chain_) { return -1; }

    if (!initScene()) { return -1; }

    showWindow();

    timer_.resume();
    return 0;
}

bool App3DMainWindow::initScene() { return device_->prepareTestScene(*surface_); }

bool App3DMainWindow::renderScene() {
    const auto result = device_->renderTestScene(*swap_chain_);
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        if (!recreateSwapChain()) { return false; }
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }
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
