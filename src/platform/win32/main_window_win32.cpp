#include "main_window.h"

#include "utils/print.h"

#include <uxs/string_util.h>

#include <windows.h>  // NOLINT

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
};

namespace {

LRESULT CALLBACK windowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY: {
            ::PostQuitMessage(0);
        } break;
        default: {
            return ::DefWindowProcW(hWnd, message, wParam, lParam);
        } break;
    }
    return 0;
}

}  // namespace

MainWindow::MainWindow() : impl_data_(std::make_unique<ImplData>()) {}

MainWindow::~MainWindow() {
    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());
    if (impl_data.hwnd) { ::DestroyWindow(impl_data.hwnd); }
    if (impl_data.hinstance) { ::UnregisterClassW(L"App3D Window Class", impl_data.hinstance); }
}

WindowDescriptor MainWindow::getWindowDescriptor() const {
    WindowDescriptor window_desc{};
    const auto& impl_data = *static_cast<const ImplData*>(impl_data_.get());
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&window_desc.v);
    win_desc_impl.hinstance = impl_data.hinstance;
    win_desc_impl.hwnd = impl_data.hwnd;
    return window_desc;
}

bool MainWindow::createWindow(const std::string& window_title, int width, int height) {
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

    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());
    impl_data.hinstance = hinstance;
    impl_data.hwnd = CreateWindowExW(0, window_class_name, uxs::wide_string_adapter{}(window_title).c_str(),
                                     WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, hinstance, NULL);
    if (!impl_data.hwnd) {
        logError("couldn't create Win32 window");
        return false;
    }

    return true;
}

void MainWindow::showWindow() {
    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());
    ::ShowWindow(impl_data.hwnd, SW_SHOWNORMAL);
    ::UpdateWindow(impl_data.hwnd);
}

int MainWindow::mainLoop() {
    MSG message;
    int ret_code = 0;

    do {
        while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            switch (message.message) {
                case WM_QUIT: {
                    return ret_code;
                } break;
                default: break;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        };
    } while (!render(ret_code));

    return ret_code;
}
