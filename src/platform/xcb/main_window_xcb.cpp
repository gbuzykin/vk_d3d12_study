#include "main_window.h"

#include "utils/print.h"

#include <xcb/xcb.h>

#include <atomic>

using namespace app3d;

// --------------------------------------------------------
// MainWindow class implementation

struct WindowDescImpl {
    xcb_connection_t* connection;
    xcb_window_t window;
};

struct MainWindow::ImplData {
    xcb_connection_t* connection = nullptr;
    xcb_screen_t* screen = nullptr;
    xcb_window_t window{};
    xcb_intern_atom_reply_t* delete_reply = nullptr;
    std::atomic<bool> finished{false};
    bool window_initialized = false;
};

MainWindow::MainWindow() : impl_data_(std::make_unique<ImplData>()) {}

MainWindow::~MainWindow() {
    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());
    if (impl_data.window_initialized) {
        ::xcb_destroy_window(impl_data.connection, impl_data.window);
        ::xcb_flush(impl_data.connection);
    }
    ::xcb_disconnect(impl_data.connection);
}

WindowDescriptor MainWindow::getWindowDescriptor() const {
    WindowDescriptor window_desc{};
    const auto& impl_data = *static_cast<const ImplData*>(impl_data_.get());
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&window_desc.v);
    win_desc_impl.connection = impl_data.connection;
    win_desc_impl.window = impl_data.window;
    return window_desc;
}

bool MainWindow::createWindow(const std::string& window_title, int width, int height) {
    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());

    // Open the connection to the X server
    impl_data.connection = ::xcb_connect(NULL, NULL);
    if (::xcb_connection_has_error(impl_data.connection)) {
        logError("couldn't open Xcb connection");
        return false;
    }

    // Get the first screen
    impl_data.screen = ::xcb_setup_roots_iterator(::xcb_get_setup(impl_data.connection)).data;
    if (!impl_data.screen) {
        logError("couldn't obtain Xcb screen");
        return false;
    }

    // Ask for our window's Id
    impl_data.window = ::xcb_generate_id(impl_data.connection);

    std::uint32_t value_mask = XCB_CW_EVENT_MASK;
    uint32_t value_list[] = {XCB_EVENT_MASK_BUTTON_PRESS};

    // Create the window
    xcb_void_cookie_t cookie = ::xcb_create_window(
        impl_data.connection, XCB_COPY_FROM_PARENT, impl_data.window, impl_data.screen->root, 0, 0, width, height, 10,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, impl_data.screen->root_visual, value_mask, value_list);

    std::unique_ptr<xcb_generic_error_t, void (*)(void*)> error{::xcb_request_check(impl_data.connection, cookie),
                                                                std::free};
    if (error) {
        logError("couldn't create Xcb window: {}", error->error_code);
        return false;
    }

    // Set the title of the window
    ::xcb_change_property(impl_data.connection, XCB_PROP_MODE_REPLACE, impl_data.window, XCB_ATOM_WM_NAME,
                          XCB_ATOM_STRING, 8, static_cast<std::uint32_t>(window_title.size()), window_title.data());

    impl_data.window_initialized = true;
    return true;
}

void MainWindow::showWindow() {
    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());

    xcb_intern_atom_reply_t* reply = ::xcb_intern_atom_reply(
        impl_data.connection, ::xcb_intern_atom(impl_data.connection, 1, 12, "WM_PROTOCOLS"), 0);

    xcb_intern_atom_reply_t* reply2 = ::xcb_intern_atom_reply(
        impl_data.connection, ::xcb_intern_atom(impl_data.connection, 0, 16, "WM_DELETE_WINDOW"), 0);

    ::xcb_change_property(impl_data.connection, XCB_PROP_MODE_REPLACE, impl_data.window, reply->atom, 4, 32, 1,
                          &reply2->atom);

    impl_data.delete_reply = reply2;

    ::xcb_map_window(impl_data.connection, impl_data.window);
    ::xcb_flush(impl_data.connection);
}

int MainWindow::mainLoop() {
    int ret_code = 0;

    auto& impl_data = *static_cast<ImplData*>(impl_data_.get());

    do {
        while (std::unique_ptr<xcb_generic_event_t, void (*)(void*)> event{::xcb_poll_for_event(impl_data.connection),
                                                                           std::free}) {
            switch (event->response_type & ~0x80) {
                case XCB_CLIENT_MESSAGE: {
                    if (reinterpret_cast<xcb_client_message_event_t*>(event.get())->data.data32[0] ==
                        impl_data.delete_reply->atom) {
                        return ret_code;
                    }
                } break;
            }
        }
    } while (!render(ret_code));

    return ret_code;
}
