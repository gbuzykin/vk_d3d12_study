#include "main_window.h"

#include "common/logger.h"

#include <uxs/string_util.h>

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstddef>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

using namespace app3d;

struct WindowDescImpl {
    HINSTANCE hinstance;
    HWND hwnd;
};

// --------------------------------------------------------
// MainWindow::Implementation class implementation

class MainWindow::Implementation {
 public:
    Implementation(MainWindow& main_window);
    ~Implementation();

    bool createWindow(const char* title, std::uint32_t width, std::uint32_t height);
    void showWindow();
    int mainLoop();

 private:
    friend class MainWindow;
    MainWindow& main_window_;

    HINSTANCE hinstance_ = NULL;
    HWND hwnd_ = NULL;
    SIZE win_size_{};
    bool is_minimized_ = false;
    bool is_deactivated_ = false;
    bool is_sizing_or_moving_ = false;
    std::array<KeyCode, 256> key_codes_{};

    void dispatchResize();
    void dispatchMouseButton(KeyCode button, bool state, std::int32_t x, std::int32_t y);
    void dispatchKeyEvent(unsigned n_key, bool state);
    LRESULT dispatchMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* impl = reinterpret_cast<MainWindow::Implementation*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        return impl->dispatchMessage(hwnd, message, wparam, lparam);
    }
};

namespace {
// clang-format off
constexpr std::array<std::uint8_t, unsigned(KeyCode::KEY_CODE_COUNT)> g_key_recode {
    0, VK_LBUTTON, VK_MBUTTON, VK_RBUTTON, 0, 0, 0, 0, 0, VK_ESCAPE, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    VK_OEM_MINUS, VK_OEM_PLUS, VK_BACK, VK_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_4, VK_OEM_6,
    VK_RETURN, VK_CONTROL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_1, VK_OEM_7, VK_OEM_3, VK_SHIFT,
    VK_OEM_5, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, 0, VK_MULTIPLY, VK_LMENU,
    VK_SPACE, VK_CAPITAL, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_NUMLOCK, VK_SCROLL,
    VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_SUBTRACT, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_ADD, VK_NUMPAD1,
    VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD0, VK_DECIMAL, 0, 0, 0, VK_F11, VK_F12, 0, 0, 0, 0, 0, 0, 0, 0, 0, VK_DIVIDE,
    VK_SNAPSHOT, VK_RMENU, 0, VK_HOME, VK_UP, VK_PRIOR, VK_LEFT, VK_RIGHT, VK_END, VK_DOWN, VK_NEXT, VK_INSERT,
    VK_DELETE, 0, VK_VOLUME_MUTE, VK_VOLUME_DOWN, VK_VOLUME_UP, 0, 0, 0, VK_PAUSE, 0, 0, 0, 0, 0, VK_LWIN, VK_RWIN,
    VK_APPS,
};
// clang-format on
}  // namespace

MainWindow::Implementation::Implementation(MainWindow& main_window) : main_window_(main_window) {
    for (unsigned n = 0; n < unsigned(KeyCode::KEY_CODE_COUNT); ++n) { key_codes_[g_key_recode[n]] = KeyCode(n); }
    key_codes_[0] = KeyCode::NONE;
}

MainWindow::Implementation::~Implementation() {
    if (hwnd_) { ::DestroyWindow(hwnd_); }
    if (hinstance_) { ::UnregisterClassW(L"App3D Window Class", hinstance_); }
}

bool MainWindow::Implementation::createWindow(const char* title, std::uint32_t width, std::uint32_t height) {
    HINSTANCE hinstance = ::GetModuleHandleW(NULL);
    const wchar_t* window_class_name = L"App3D Window Class";
    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = MainWindow::Implementation::windowProcedure,
        .hInstance = hinstance,
        .hCursor = ::LoadCursorA(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = window_class_name,
    };

    if (!::RegisterClassExW(&window_class)) {
        logError("couldn't register Win32 window class");
        return false;
    }

    RECT win_rect{.right = LONG(width), .bottom = LONG(height)};

    ::AdjustWindowRect(&win_rect, WS_OVERLAPPEDWINDOW, FALSE);

    std::uint32_t screen_width = 0;
    std::uint32_t screen_height = 0;

    HMONITOR hmonitor = ::MonitorFromPoint(POINT{0, 0}, 0);
    if (hmonitor != INVALID_HANDLE_VALUE) {
        MONITORINFO info{.cbSize = sizeof(MONITORINFO)};
        if (::GetMonitorInfoW(hmonitor, &info) && info.rcMonitor.left == 0 && info.rcMonitor.top == 0) {
            screen_width = info.rcMonitor.right;
            screen_height = info.rcMonitor.bottom;
        }
    }

    if (screen_width && screen_height) {
        const std::uint32_t offset_x = std::max<std::uint32_t>((screen_width - width) / 4, 0);
        const std::uint32_t offset_y = std::max<std::uint32_t>((screen_height - height) / 4, 0);
        win_rect.left += offset_x, win_rect.right += offset_x;
        win_rect.top += offset_y, win_rect.bottom += offset_y;
    }

    win_size_.cx = win_rect.right - win_rect.left;
    win_size_.cy = win_rect.bottom - win_rect.top;

    hinstance_ = hinstance;
    hwnd_ = ::CreateWindowExW(0, window_class_name, uxs::wide_string_adapter{}(title).c_str(), WS_OVERLAPPEDWINDOW,
                              win_rect.left, win_rect.top, win_size_.cx, win_size_.cy, NULL, NULL, hinstance, NULL);
    if (!hwnd_) {
        logError("couldn't create Win32 window");
        return false;
    }

    ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    return true;
}

void MainWindow::Implementation::showWindow() {
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::UpdateWindow(hwnd_);
}

int MainWindow::Implementation::mainLoop() {
    MSG message;
    while (!main_window_.terminate_) {
        if (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { return 0; }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        } else {
            main_window_.onIdle();
        }
    }
    return main_window_.ret_code_;
}

void MainWindow::Implementation::dispatchResize() {
    RECT win_rect{};
    if (is_minimized_ || (::GetWindowRect(hwnd_, &win_rect) && (win_rect.right - win_rect.left != win_size_.cx ||
                                                                win_rect.bottom - win_rect.top != win_size_.cy))) {
        is_minimized_ = false;
        win_size_.cx = win_rect.right - win_rect.left;
        win_size_.cy = win_rect.bottom - win_rect.top;
        main_window_.onResize();
    }
}

void MainWindow::Implementation::dispatchMouseButton(KeyCode button, bool state, std::int32_t x, std::int32_t y) {
    if (state) {
        ::SetCapture(hwnd_);
    } else {
        ::ReleaseCapture();
    }
    main_window_.onMouseButtonEvent(button, state, x, y);
}

void MainWindow::Implementation::dispatchKeyEvent(unsigned n_key, bool state) {
    if (n_key >= key_codes_.size()) { return; }
    const KeyCode code = key_codes_[n_key];
    if (code != KeyCode::NONE) { main_window_.onKeyEvent(code, state); }
}

LRESULT MainWindow::Implementation::dispatchMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    const auto get_mouse_button_mask = [](WPARAM state) -> BYTE {
        BYTE button_mask = 0;
        if (state & MK_LBUTTON) { button_mask |= 1; }
        if (state & MK_MBUTTON) { button_mask |= 2; }
        if (state & MK_RBUTTON) { button_mask |= 4; }
        return button_mask;
    };

    switch (message) {
        case WM_ACTIVATE: {
            const bool is_deactivated = LOWORD(wparam) == WA_INACTIVE;
            if (is_deactivated != is_deactivated_) {
                is_deactivated_ = is_deactivated;
                main_window_.onEvent(is_deactivated ? MainWindow::Event::DEACTIVATE : MainWindow::Event::ACTIVATE);
            }
        } break;
        case WM_ENTERSIZEMOVE: {
            if (!is_sizing_or_moving_) {
                is_sizing_or_moving_ = true;
                main_window_.onEvent(MainWindow::Event::ENTER_SIZING_OR_MOVING);
            }
        } break;
        case WM_EXITSIZEMOVE: {
            if (is_sizing_or_moving_) {
                is_sizing_or_moving_ = false;
                dispatchResize();
                if (main_window_.terminate_) { return 0; }
                main_window_.onEvent(MainWindow::Event::EXIT_SIZING_OR_MOVING);
            }
        } break;
        case WM_SIZE: {
            if (wparam == SIZE_MINIMIZED) {
                if (!is_minimized_) {
                    is_minimized_ = true;
                    main_window_.onEvent(MainWindow::Event::MINIMIZE);
                }
            } else if (!is_sizing_or_moving_) {
                dispatchResize();
            }
        } break;
        case WM_LBUTTONDOWN: {
            dispatchMouseButton(KeyCode::MOUSE_LBUTTON, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_LBUTTONUP: {
            dispatchMouseButton(KeyCode::MOUSE_LBUTTON, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_MBUTTONDOWN: {
            dispatchMouseButton(KeyCode::MOUSE_MBUTTON, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_MBUTTONUP: {
            dispatchMouseButton(KeyCode::MOUSE_MBUTTON, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_RBUTTONDOWN: {
            dispatchMouseButton(KeyCode::MOUSE_RBUTTON, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_RBUTTONUP: {
            dispatchMouseButton(KeyCode::MOUSE_RBUTTON, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        } break;
        case WM_MOUSEWHEEL: {
            const std::int16_t delta = std::int16_t(HIWORD(wparam));
            main_window_.onMouseWheel(delta * 0.002f, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                                      get_mouse_button_mask(wparam));
        } break;
        case WM_MOUSEMOVE: {
            main_window_.onMouseMove(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), get_mouse_button_mask(wparam));
        } break;
        case WM_KEYDOWN: {
            if (!(lparam & (1 << 30))) { dispatchKeyEvent(wparam, true); }
        } break;
        case WM_KEYUP: {
            dispatchKeyEvent(wparam, false);
        } break;
        case WM_DESTROY: {
            ::PostQuitMessage(0);
        } break;
        default: {
            return ::DefWindowProcW(hwnd, message, wparam, lparam);
        } break;
    }
    return 0;
}

// --------------------------------------------------------
// MainWindow::Implementation class implementation

MainWindow::MainWindow() : impl_(std::make_unique<Implementation>(*this)) {}

MainWindow::~MainWindow() {}

rel::WindowDescriptor MainWindow::getWindowDescriptor() const {
    rel::WindowDescriptor win_desc{.platform = rel::PlatformType::PLATFORM_WIN32};
    static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
    static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                  "Too little WindowDescriptor alignment");
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&win_desc.handle);
    win_desc_impl.hinstance = impl_->hinstance_;
    win_desc_impl.hwnd = impl_->hwnd_;
    return win_desc;
}

bool MainWindow::createWindow(const char* title, std::uint32_t width, std::uint32_t height) {
    return impl_->createWindow(title, width, height);
}

void MainWindow::showWindow() { return impl_->showWindow(); }

int MainWindow::mainLoop() { return impl_->mainLoop(); }
