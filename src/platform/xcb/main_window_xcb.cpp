#include "main_window.h"

#include "common/logger.h"

#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>

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
    std::unique_ptr<xcb_intern_atom_reply_t, void (*)(void*)> delete_window_atom{nullptr, std::free};
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    bool window_initialized = false;

    struct KeyState {
        bool release;
        std::chrono::high_resolution_clock::time_point release_timer_start;
    };
    std::array<KeyState, unsigned(KeyCode::KEY_CODE_COUNT)> key_state_table{};
    bool check_key_timers = false;
};

MainWindow::MainWindow() : data_(std::make_unique<ImplData>()) {}

MainWindow::~MainWindow() {
    if (data_->window_initialized) {
        ::xcb_destroy_window(data_->connection, data_->window);
        ::xcb_flush(data_->connection);
    }
    ::xcb_disconnect(data_->connection);
}

rel::WindowDescriptor MainWindow::getWindowDescriptor() const {
    rel::WindowDescriptor win_desc{.platform = rel::PlatformType::PLATFORM_XCB};
    static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
    static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                  "Too little WindowDescriptor alignment");
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&win_desc.handle);
    win_desc_impl.connection = data_->connection;
    win_desc_impl.window = data_->window;
    return win_desc;
}

bool MainWindow::createWindow(const std::string& title, std::uint32_t width, std::uint32_t height) {
    // Open the connection to the X server
    data_->connection = ::xcb_connect(NULL, NULL);
    if (::xcb_connection_has_error(data_->connection)) {
        logError("couldn't open Xcb connection");
        return false;
    }

    // Get the first screen
    data_->screen = ::xcb_setup_roots_iterator(::xcb_get_setup(data_->connection)).data;
    if (!data_->screen) {
        logError("couldn't obtain Xcb screen");
        return false;
    }

    data_->width = std::uint16_t(width);
    data_->height = std::uint16_t(height);

    // Ask for our window's Id
    data_->window = ::xcb_generate_id(data_->connection);

    std::uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    std::uint32_t value_list[2] = {data_->screen->white_pixel,
                                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS |
                                       XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
                                       XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION};

    // Create the window
    xcb_void_cookie_t cookie = ::xcb_create_window(
        data_->connection, XCB_COPY_FROM_PARENT, data_->window, data_->screen->root, 0, 0, data_->width, data_->height,
        10, XCB_WINDOW_CLASS_INPUT_OUTPUT, data_->screen->root_visual, value_mask, value_list);

    std::unique_ptr<xcb_generic_error_t, void (*)(void*)> error{::xcb_request_check(data_->connection, cookie),
                                                                std::free};
    if (error) {
        logError("couldn't create Xcb window: {}", error->error_code);
        return false;
    }

    // Set the title of the window
    ::xcb_change_property(data_->connection, XCB_PROP_MODE_REPLACE, data_->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                          std::uint32_t(title.size()), title.data());

    data_->window_initialized = true;
    return true;
}

void MainWindow::showWindow() {
    std::unique_ptr<xcb_intern_atom_reply_t, void (*)(void*)> protocols_atom{
        ::xcb_intern_atom_reply(data_->connection, ::xcb_intern_atom(data_->connection, 1, 12, "WM_PROTOCOLS"), 0),
        std::free};

    data_->delete_window_atom.reset(
        ::xcb_intern_atom_reply(data_->connection, ::xcb_intern_atom(data_->connection, 0, 16, "WM_DELETE_WINDOW"), 0));

    ::xcb_change_property(data_->connection, XCB_PROP_MODE_REPLACE, data_->window, protocols_atom->atom, 4, 32, 1,
                          &data_->delete_window_atom->atom);

    ::xcb_map_window(data_->connection, data_->window);

    std::int32_t values[2] = {
        std::max<std::int16_t>(std::int16_t(data_->screen->width_in_pixels - data_->width) / 4, 0),
        std::max<std::int16_t>(std::int16_t(data_->screen->height_in_pixels - data_->height) / 4, 0),
    };

    ::xcb_configure_window(data_->connection, data_->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);

    ::xcb_flush(data_->connection);
}

int MainWindow::mainLoop() {
    int ret_code = 0;

    while (true) {
        if (data_->check_key_timers) {
            bool has_pressed = false;
            const auto timer_now = std::chrono::high_resolution_clock::now();
            for (std::uint32_t n = 0; n < data_->key_state_table.size(); ++n) {
                auto& key_state = data_->key_state_table[n];
                if (key_state.release) {
                    if (std::chrono::duration<double>(timer_now - key_state.release_timer_start).count() > 0.01) {
                        key_state.release = false;
                        if (!onKeyEvent(static_cast<KeyCode>(n), false, ret_code)) { return ret_code; }
                    } else {
                        has_pressed = true;
                    }
                }
            }
            if (!has_pressed) { data_->check_key_timers = false; }
        }

        if (std::unique_ptr<xcb_generic_event_t, void (*)(void*)> event{::xcb_poll_for_event(data_->connection),
                                                                        std::free}) {
            const auto get_mouse_button_mask = [](std::uint8_t state) -> std::uint8_t {
                const unsigned button1_bit_num = 8;
                const std::uint8_t button_mask = 7;
                return (state >> button1_bit_num) & button_mask;
            };

            switch (event->response_type & ~0x80) {
                case XCB_CLIENT_MESSAGE: {
                    auto* ev = reinterpret_cast<xcb_client_message_event_t*>(event.get());
                    if (ev->data.data32[0] == data_->delete_window_atom->atom) { return ret_code; }
                } break;
                case XCB_CONFIGURE_NOTIFY: {
                    auto* ev = reinterpret_cast<xcb_configure_notify_event_t*>(event.get());
                    if (ev->width != data_->width || ev->height != data_->height) {
                        data_->width = ev->width;
                        data_->height = ev->height;
                        if (!onResize(ret_code)) { return ret_code; };
                    }
                } break;
                case XCB_KEY_PRESS: {
                    auto* ev = reinterpret_cast<xcb_key_press_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail < data_->key_state_table.size()) {
                        auto& key_state = data_->key_state_table[ev->detail];
                        if (!key_state.release) {
                            if (!onKeyEvent(static_cast<KeyCode>(ev->detail), true, ret_code)) { return ret_code; }
                        }
                        key_state.release = false;
                    }
                } break;
                case XCB_KEY_RELEASE: {
                    auto* ev = reinterpret_cast<xcb_key_release_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail < data_->key_state_table.size()) {
                        auto& key_state = data_->key_state_table[ev->detail];
                        key_state.release = true;
                        key_state.release_timer_start = std::chrono::high_resolution_clock::now();
                        data_->check_key_timers = true;
                    }
                    break;
                }
                case XCB_BUTTON_PRESS: {
                    auto* ev = reinterpret_cast<xcb_button_press_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail <= 3) {
                        onMouseButtonEvent(static_cast<KeyCode>(ev->detail), true, ev->event_x, ev->event_y);
                    } else if (ev->detail == 4 || ev->detail == 5) {
                        onMouseWheel(ev->detail == 4 ? 0.24f : -0.24f, ev->event_x, ev->event_y,
                                     get_mouse_button_mask(ev->state));
                    }
                } break;
                case XCB_BUTTON_RELEASE: {
                    auto* ev = reinterpret_cast<xcb_button_release_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail <= 3) {
                        onMouseButtonEvent(static_cast<KeyCode>(ev->detail), false, ev->event_x, ev->event_y);
                    }
                } break;
                case XCB_MOTION_NOTIFY: {
                    auto* ev = reinterpret_cast<xcb_motion_notify_event_t*>(event.get());
                    onMouseMove(ev->event_x, ev->event_y, get_mouse_button_mask(ev->state));
                } break;
            }
        } else if (!onIdle(ret_code)) {
            return ret_code;
        }
    }
}
