#include "main_window.h"

#include "common/logger.h"

#include <uxs/string_util.h>

#include <windows.h>
#include <windowsx.h>

#include <algorithm>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

using namespace app3d;

// --------------------------------------------------------
// MainWindow class implementation

struct WindowDescImpl {
    HINSTANCE hinstance;
    HWND hwnd;
};

struct MainWindow::ImplData {
    HINSTANCE hinstance = NULL;
    HWND hwnd = NULL;
    SIZE win_size{};
    std::array<KeyCode, 256> key_codes{};
};

namespace {

enum UserMessage {
    USER_MESSAGE_RESIZE = WM_USER + 1,
    USER_MESSAGE_KEY_EVENT,
    USER_MESSAGE_MOUSE_EVENT,
};

LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    const auto get_mouse_button_mask = [](WPARAM state) -> BYTE {
        BYTE button_mask = 0;
        if (state & MK_LBUTTON) { button_mask |= 1; }
        if (state & MK_MBUTTON) { button_mask |= 2; }
        if (state & MK_RBUTTON) { button_mask |= 4; }
        return button_mask;
    };

    switch (message) {
        case WM_LBUTTONDOWN: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(1, 1), lparam);
        } break;
        case WM_LBUTTONUP: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(1, 0), lparam);
        } break;
        case WM_MBUTTONDOWN: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(2, 1), lparam);
        } break;
        case WM_MBUTTONUP: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(2, 0), lparam);
        } break;
        case WM_RBUTTONDOWN: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(3, 1), lparam);
        } break;
        case WM_RBUTTONUP: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWPARAM(3, 0), lparam);
        } break;
        case WM_MOUSEWHEEL: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT,
                           MAKEWPARAM(MAKEWORD(4, get_mouse_button_mask(wparam)), HIWORD(wparam)), lparam);
        } break;
        case WM_MOUSEMOVE: {
            ::PostMessageW(hwnd, USER_MESSAGE_MOUSE_EVENT, MAKEWORD(0, get_mouse_button_mask(wparam)), lparam);
        } break;
        case WM_KEYDOWN: {
            if (!(lparam & (1 << 30))) { ::PostMessageW(hwnd, USER_MESSAGE_KEY_EVENT, wparam, 1); }
        } break;
        case WM_KEYUP: {
            ::PostMessageW(hwnd, USER_MESSAGE_KEY_EVENT, wparam, 0);
        } break;
        case WM_EXITSIZEMOVE:
        case WM_SIZE: {
            ::PostMessageW(hwnd, USER_MESSAGE_RESIZE, wparam, lparam);
        } break;
            break;
        case WM_DESTROY: {
            ::PostQuitMessage(0);
        } break;
        default: {
            return ::DefWindowProcW(hwnd, message, wparam, lparam);
        } break;
    }
    return 0;
}

}  // namespace

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

MainWindow::MainWindow() : data_(std::make_unique<ImplData>()) {
    for (unsigned n = 0; n < unsigned(KeyCode::KEY_CODE_COUNT); ++n) { data_->key_codes[g_key_recode[n]] = KeyCode(n); }
    data_->key_codes[0] = KeyCode::NONE;
}

MainWindow::~MainWindow() {
    if (data_->hwnd) { ::DestroyWindow(data_->hwnd); }
    if (data_->hinstance) { ::UnregisterClassW(L"App3D Window Class", data_->hinstance); }
}

rel::WindowDescriptor MainWindow::getWindowDescriptor() const {
    rel::WindowDescriptor win_desc{.platform = rel::PlatformType::PLATFORM_WIN32};
    static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
    static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                  "Too little WindowDescriptor alignment");
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&win_desc.handle);
    win_desc_impl.hinstance = data_->hinstance;
    win_desc_impl.hwnd = data_->hwnd;
    return win_desc;
}

bool MainWindow::createWindow(const std::string& title, std::uint32_t width, std::uint32_t height) {
    HINSTANCE hinstance = ::GetModuleHandleW(NULL);
    const wchar_t* window_class_name = L"App3D Window Class";
    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = windowProcedure,
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

    data_->win_size.cx = win_rect.right - win_rect.left;
    data_->win_size.cy = win_rect.bottom - win_rect.top;

    data_->hinstance = hinstance;
    data_->hwnd = ::CreateWindowExW(0, window_class_name, uxs::wide_string_adapter{}(title).c_str(),
                                    WS_OVERLAPPEDWINDOW, win_rect.left, win_rect.top, data_->win_size.cx,
                                    data_->win_size.cy, NULL, NULL, hinstance, NULL);
    if (!data_->hwnd) {
        logError("couldn't create Win32 window");
        return false;
    }

    return true;
}

void MainWindow::showWindow() {
    ::ShowWindow(data_->hwnd, SW_SHOWNORMAL);
    ::UpdateWindow(data_->hwnd);
}

int MainWindow::mainLoop() {
    MSG message;
    int ret_code = 0;

    while (true) {
        if (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            switch (message.message) {
                case USER_MESSAGE_RESIZE: {
                    RECT win_rect{};
                    if (::GetWindowRect(data_->hwnd, &win_rect) &&
                        (win_rect.right - win_rect.left != data_->win_size.cx ||
                         win_rect.bottom - win_rect.top != data_->win_size.cy)) {
                        data_->win_size.cx = win_rect.right - win_rect.left;
                        data_->win_size.cy = win_rect.bottom - win_rect.top;
                        if (!onResize(ret_code)) { return ret_code; }
                    }
                } break;
                case USER_MESSAGE_MOUSE_EVENT: {
                    const std::uint8_t n_button = LOBYTE(LOWORD(message.wParam));
                    if (n_button == 0) {
                        onMouseMove(GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam),
                                    HIBYTE(LOWORD(message.wParam)));
                    }
                    if (n_button >= 1 && n_button <= 3) {
                        onMouseButtonEvent(KeyCode(n_button), HIWORD(message.wParam), GET_X_LPARAM(message.lParam),
                                           GET_Y_LPARAM(message.lParam));
                    } else if (n_button == 4) {
                        const std::int16_t delta = std::int16_t(HIWORD(message.wParam));
                        onMouseWheel(delta * 0.002f, GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam),
                                     HIBYTE(LOWORD(message.wParam)));
                    }
                } break;
                case USER_MESSAGE_KEY_EVENT: {
                    const KeyCode code = data_->key_codes[message.wParam];
                    if (code != KeyCode::NONE) {
                        if (!onKeyEvent(code, message.lParam, ret_code)) { return ret_code; }
                    }
                } break;
                case WM_QUIT: {
                    return ret_code;
                } break;
                default: break;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        } else if (!onIdle(ret_code)) {
            return ret_code;
        }
    }
}
