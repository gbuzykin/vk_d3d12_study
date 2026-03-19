#pragma once

#include "interfaces/i_rendering_driver.h"

#include <string>

namespace app3d {

// clang-format off
enum class KeyCode : std::uint8_t {
    NONE = 0, MOUSE_LBUTTON, MOUSE_MBUTTON, MOUSE_RBUTTON, MOUSE_WHEEL_UP, MOUSE_WHEEL_DOWN, UNKNOWN_6, UNKNOWN_7,
    UNKNOWN_8, KEY_ESCAPE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL,
    KEY_BACKSPACE, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, BRACKET_LEFT,
    BRACKET_RIGHT, KEY_ENTER, CTRL_LEFT, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,
    KEY_QUOTE, KEY_BACKQUOTE, SHIFT_LEFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA,
    KEY_PERIOD, KEY_SLASH, SHIFT_RIGHT, NUMPAD_MULTIPLY, ALT_LEFT, KEY_SPACE, CAPS_LOCK, KEY_F1, KEY_F2, KEY_F3,
    KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, NUM_LOCK, SCROLL_LOCK, NUMPAD_7, NUMPAD_8, NUMPAD_9,
    NUMPAD_SUBTRACT, NUMPAD_4, NUMPAD_5, NUMPAD_6, NUMPAD_ADD, NUMPAD_1, NUMPAD_2, NUMPAD_3, NUMPAD_0, NUMPAD_DECIMAL,
    UNKNOWN_92, UNKNOWN_93, UNKNOWN_94, KEY_F11, KEY_F12, UNKNOWN_97, UNKNOWN_98, UNKNOWN_99, UNKNOWN_100,
    UNKNOWN_101, UNKNOWN_102, UNKNOWN_103, NUMPAD_ENTER, CTRL_RIGHT, NUMPAD_DIVIDE, PRINT_SCREEN, ALT_RIGHT,
    UNKNOWN_109, KEY_HOME, ARROW_UP, PAGE_UP, ARROW_LEFT, ARROW_RIGHT, KEY_END, ARROW_DOWN, PAGE_DOWN, KEY_INSERT,
    KEY_DELETE, UNKNOWN_120, VOLUME_MUTE, VOLUME_DOWN, VOLUME_UP, UNKNOWN_124, NUMPAD_EQUAL, UNKNOWN_126, KEY_PAUSE,
    UNKNOWN_128, NUMPAD_COMMA, UNKNOWN_130, UNKNOWN_131, UNKNOWN_132, WIN_LEFT, WIN_RIGHT, CONTEXT_MENU, KEY_CODE_COUNT
};
// clang-format on

class MainWindow {
 public:
    MainWindow();
    virtual ~MainWindow();

    rel::WindowDescriptor getWindowDescriptor() const;

    bool createWindow(const std::string& title, std::uint32_t width, std::uint32_t height);
    void showWindow();
    int mainLoop();

 protected:
    virtual bool onIdle(int& ret_code) { return true; }
    virtual bool onResize(int& ret_code) { return true; }
    virtual bool onKeyEvent(KeyCode key, bool state, int& ret_code) { return true; }
    virtual void onMouseButtonEvent(KeyCode button, bool state, std::int32_t x, std::int32_t y) {}
    virtual void onMouseWheel(float distance, std::int32_t x, std::int32_t y, std::uint8_t button_mask) {}
    virtual void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) {}

 private:
    struct ImplData;
    std::unique_ptr<ImplData> data_;
};

}  // namespace app3d
