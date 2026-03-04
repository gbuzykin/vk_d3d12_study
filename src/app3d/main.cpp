#include "main_window.h"

#include "utils/dynamic_library.h"
#include "utils/logger.h"

#include <exception>

using namespace app3d;

const char* key_text[] = {
    "NONE",        "MOUSE_LBUTTON",  "MOUSE_MBUTTON", "MOUSE_RBUTTON",   "MOUSE_WHEEL_UP",  "MOUSE_WHEEL_DOWN",
    "UNKNOWN_6",   "UNKNOWN_7",      "UNKNOWN_8",     "KEY_ESCAPE",      "KEY_1",           "KEY_2",
    "KEY_3",       "KEY_4",          "KEY_5",         "KEY_6",           "KEY_7",           "KEY_8",
    "KEY_9",       "KEY_0",          "KEY_MINUS",     "KEY_EQUAL",       "KEY_BACKSPACE",   "KEY_TAB",
    "KEY_Q",       "KEY_W",          "KEY_E",         "KEY_R",           "KEY_T",           "KEY_Y",
    "KEY_U",       "KEY_I",          "KEY_O",         "KEY_P",           "BRACKET_LEFT",    "BRACKET_RIGHT",
    "KEY_ENTER",   "CTRL_LEFT",      "KEY_A",         "KEY_S",           "KEY_D",           "KEY_F",
    "KEY_G",       "KEY_H",          "KEY_J",         "KEY_K",           "KEY_L",           "KEY_SEMICOLON",
    "KEY_QUOTE",   "KEY_BACKQUOTE",  "SHIFT_LEFT",    "KEY_BACKSLASH",   "KEY_Z",           "KEY_X",
    "KEY_C",       "KEY_V",          "KEY_B",         "KEY_N",           "KEY_M",           "KEY_COMMA",
    "KEY_PERIOD",  "KEY_SLASH",      "SHIFT_RIGHT",   "NUMPAD_MULTIPLY", "ALT_LEFT",        "KEY_SPACE",
    "CAPS_LOCK",   "KEY_F1",         "KEY_F2",        "KEY_F3",          "KEY_F4",          "KEY_F5",
    "KEY_F6",      "KEY_F7",         "KEY_F8",        "KEY_F9",          "KEY_F10",         "NUM_LOCK",
    "SCROLL_LOCK", "NUMPAD_7",       "NUMPAD_8",      "NUMPAD_9",        "NUMPAD_SUBTRACT", "NUMPAD_4",
    "NUMPAD_5",    "NUMPAD_6",       "NUMPAD_ADD",    "NUMPAD_1",        "NUMPAD_2",        "NUMPAD_3",
    "NUMPAD_0",    "NUMPAD_DECIMAL", "UNKNOWN_92",    "UNKNOWN_93",      "UNKNOWN_94",      "KEY_F11",
    "KEY_F12",     "UNKNOWN_97",     "UNKNOWN_98",    "UNKNOWN_99",      "UNKNOWN_100",     "UNKNOWN_101",
    "UNKNOWN_102", "UNKNOWN_103",    "NUMPAD_ENTER",  "CTRL_RIGHT",      "NUMPAD_DIVIDE",   "PRINT_SCREEN",
    "ALT_RIGHT",   "UNKNOWN_109",    "KEY_HOME",      "ARROW_UP",        "PAGE_UP",         "ARROW_LEFT",
    "ARROW_RIGHT", "KEY_END",        "ARROW_DOWN",    "PAGE_DOWN",       "KEY_INSERT",      "KEY_DELETE",
    "UNKNOWN_120", "VOLUME_MUTE",    "VOLUME_DOWN",   "VOLUME_UP",       "UNKNOWN_124",     "NUMPAD_EQUAL",
    "UNKNOWN_126", "KEY_PAUSE",      "UNKNOWN_128",   "NUMPAD_COMMA",    "UNKNOWN_130",     "UNKNOWN_131",
    "UNKNOWN_132", "WIN_LEFT",       "WIN_RIGHT",     "CONTEXT_MENU",    "KEY_CODE_COUNT"};

class App3DMainWindow : public MainWindow {
 public:
    int init(int argc, char** argv);

 protected:
    bool onResize(int& ret_code) {
        logInfo("resize");
        return true;
    }

    bool onKeyEvent(KeyCode key, unsigned code, bool state, int& ret_code) {
        logInfo("key {} ({:x}) {}", key_text[unsigned(key)], code, state ? "pressed" : "unpressed");
        return true;
    }

    void onMouseButtonEvent(KeyCode button, bool state, std::int32_t x, std::int32_t y) {
        logInfo("mouse button {} {}, x = {}, y = {}", unsigned(button), state ? "pressed" : "unpressed", x, y);
    }

    void onMouseWheel(float distance, std::int32_t x, std::int32_t y, std::uint8_t button_mask) {
        logInfo("mouse wheel {}, x = {}, y = {}, mask = {}", distance, x, y, button_mask);
    }

    void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) {
        //   logInfo("mouse move, x = {}, y = {}, mask = {}", x, y, button_mask);
    }

 private:
    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::ISurface* surface_ = nullptr;
    rel::IDevice* device_ = nullptr;
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
    rel::DesiredDeviceCaps caps{};

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, caps)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    device_ = driver_->createDevice(device_index, caps);
    if (!device_) { return -1; }

    rel::SwapChainCreateInfo create_info{};
    swap_chain_ = device_->createSwapChain(*surface_, create_info);
    if (!swap_chain_) { return -1; }

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
