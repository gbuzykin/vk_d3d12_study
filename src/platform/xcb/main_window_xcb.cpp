#include "main_window.h"

#include "common/logger.h"

#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace app3d;

struct WindowDescImpl {
    xcb_connection_t* connection;
    xcb_window_t window;
};

class MainWindow::Implementation {
 public:
    explicit Implementation(MainWindow& main_window) : main_window_(main_window) {}
    ~Implementation();

    bool createWindow(const char* title, std::uint32_t width, std::uint32_t height);
    void showWindow();
    int mainLoop();

 private:
    friend class MainWindow;
    MainWindow& main_window_;

    xcb_connection_t* connection_ = nullptr;
    xcb_screen_t* screen_ = nullptr;
    xcb_window_t window_{};
    xcb_atom_t wm_delete_window_;
    xcb_atom_t wm_state_;
    xcb_atom_t wm_state_hidden_;
    xcb_atom_t wm_state_focused_;
    xcb_rectangle_t window_rect_;
    bool window_initialized_ = false;

    struct KeyState {
        bool release = false;
        std::chrono::high_resolution_clock::time_point release_timer_start{};
    };

    bool check_key_timers_ = false;
    std::array<KeyState, unsigned(KeyCode::KEY_CODE_COUNT)> key_state_table_{};

    bool is_minimized_ = false;
    bool is_deactivated_ = false;
    bool is_moving_ = false;
    bool is_sizing_ = false;
    bool is_sizing_or_moving_ = false;
    std::chrono::high_resolution_clock::time_point moving_timer_start_{};
    std::chrono::high_resolution_clock::time_point sizing_timer_start_{};

    xcb_atom_t queryAtom(std::string_view name);
    void getPropertyValues(xcb_atom_t property, std::vector<xcb_atom_t>& values);
};

// --------------------------------------------------------
// MainWindow::Implementation class implementation

MainWindow::Implementation::~Implementation() {
    if (window_initialized_) {
        ::xcb_destroy_window(connection_, window_);
        ::xcb_flush(connection_);
    }
    ::xcb_disconnect(connection_);
}

bool MainWindow::Implementation::createWindow(const char* title, std::uint32_t width, std::uint32_t height) {
    // Open the connection to the X server
    connection_ = ::xcb_connect(NULL, NULL);
    if (::xcb_connection_has_error(connection_)) {
        logError("couldn't open Xcb connection");
        return false;
    }

    // Get the first screen
    screen_ = ::xcb_setup_roots_iterator(::xcb_get_setup(connection_)).data;
    if (!screen_) {
        logError("couldn't obtain Xcb screen");
        return false;
    }

    window_rect_ = xcb_rectangle_t{
        .x = std::max<std::int16_t>(std::int16_t(screen_->width_in_pixels - std::uint16_t(width)) / 4, 0),
        .y = std::max<std::int16_t>(std::int16_t(screen_->height_in_pixels - std::uint16_t(height)) / 4, 0),
        .width = std::uint16_t(width),
        .height = std::uint16_t(height),
    };

    // Ask for our window's Id
    window_ = ::xcb_generate_id(connection_);

    std::uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    std::uint32_t value_list[2] = {
        screen_->white_pixel, XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE |
                                  XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
                                  XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION};

    // Create the window
    xcb_void_cookie_t cookie = ::xcb_create_window(
        connection_, XCB_COPY_FROM_PARENT, window_, screen_->root, 0, 0, window_rect_.width, window_rect_.height, 2,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen_->root_visual, value_mask, value_list);

    std::unique_ptr<xcb_generic_error_t, void (*)(void*)> err{::xcb_request_check(connection_, cookie), std::free};
    if (err) {
        logError("couldn't create Xcb window: {}", err->error_code);
        return false;
    }

    // Set the title of the window
    ::xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, window_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                          std::uint32_t(std::strlen(title)), title);

    window_initialized_ = true;
    return true;
}

void MainWindow::Implementation::showWindow() {
    const xcb_atom_t protocols_atom = queryAtom("WM_PROTOCOLS");
    wm_delete_window_ = queryAtom("WM_DELETE_WINDOW");
    wm_state_ = queryAtom("_NET_WM_STATE");
    wm_state_hidden_ = queryAtom("_NET_WM_STATE_HIDDEN");
    wm_state_focused_ = queryAtom("_NET_WM_STATE_FOCUSED");

    ::xcb_change_property(connection_, XCB_PROP_MODE_REPLACE, window_, protocols_atom, XCB_ATOM_ATOM, 32, 1,
                          &wm_delete_window_);

    ::xcb_map_window(connection_, window_);

    std::int32_t values[2] = {window_rect_.x, window_rect_.y};

    ::xcb_configure_window(connection_, window_, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);

    ::xcb_flush(connection_);
}

#define CALL_AND_CHECK_RESULT(call) \
    { \
        main_window_.call; \
        if (main_window_.terminate_) { return main_window_.ret_code_; } \
    }

int MainWindow::Implementation::mainLoop() {
    std::vector<xcb_atom_t> prop_values;
    prop_values.reserve(16);

    while (true) {
        if (check_key_timers_) {
            bool has_pressed = false;
            const auto timer_now = std::chrono::high_resolution_clock::now();
            for (std::uint32_t n = 0; n < key_state_table_.size(); ++n) {
                auto& key_state = key_state_table_[n];
                if (key_state.release) {
                    if (std::chrono::duration<double>(timer_now - key_state.release_timer_start).count() >= 0.01) {
                        key_state.release = false;
                        CALL_AND_CHECK_RESULT(onKeyEvent(static_cast<KeyCode>(n), false));
                    } else {
                        has_pressed = true;
                    }
                }
            }
            if (!has_pressed) { check_key_timers_ = false; }
        }

        if (is_moving_) {
            const auto timer_now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double>(timer_now - moving_timer_start_).count() >= 0.25) { is_moving_ = false; }
        }

        if (is_sizing_) {
            const auto timer_now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double>(timer_now - sizing_timer_start_).count() >= 0.25) {
                is_sizing_ = false;
                if (window_rect_.width != 0 && window_rect_.height != 0) {
                    is_minimized_ = false;
                    CALL_AND_CHECK_RESULT(onResize());
                } else if (!is_minimized_) {
                    is_minimized_ = true;
                    CALL_AND_CHECK_RESULT(onEvent(Event::MINIMIZE));
                }
            }
        }

        if (!is_moving_ && !is_sizing_ && is_sizing_or_moving_) {
            is_sizing_or_moving_ = false;
            CALL_AND_CHECK_RESULT(onEvent(Event::EXIT_SIZING_OR_MOVING));
        }

        if (std::unique_ptr<xcb_generic_event_t, void (*)(void*)> event{::xcb_poll_for_event(connection_), std::free}) {
            const auto get_mouse_button_mask = [](std::uint8_t state) -> std::uint8_t {
                const unsigned button1_bit_num = 8;
                const std::uint8_t button_mask = 7;
                return (state >> button1_bit_num) & button_mask;
            };

            switch (event->response_type & ~0x80) {
                case XCB_CLIENT_MESSAGE: {
                    auto* ev = reinterpret_cast<xcb_client_message_event_t*>(event.get());
                    if (ev->data.data32[0] == wm_delete_window_) { return 0; }
                } break;

                case XCB_PROPERTY_NOTIFY: {
                    auto* ev = reinterpret_cast<xcb_property_notify_event_t*>(event.get());
                    if (ev->atom == wm_state_) {
                        getPropertyValues(wm_state_, prop_values);
                        if (std::ranges::find(prop_values, wm_state_hidden_) != prop_values.end()) {
                            is_sizing_ = false;
                            if (!is_minimized_) {
                                is_minimized_ = true;
                                CALL_AND_CHECK_RESULT(onEvent(Event::MINIMIZE));
                            }
                        } else if (is_minimized_) {
                            is_minimized_ = false;
                            CALL_AND_CHECK_RESULT(onResize());
                        }
                        if (std::ranges::find(prop_values, wm_state_focused_) != prop_values.end()) {
                            if (is_deactivated_) {
                                is_deactivated_ = false;
                                CALL_AND_CHECK_RESULT(onEvent(Event::ACTIVATE));
                            }
                        } else if (!is_deactivated_) {
                            is_deactivated_ = true;
                            CALL_AND_CHECK_RESULT(onEvent(Event::DEACTIVATE));
                        }
                    }
                } break;

                case XCB_CONFIGURE_NOTIFY: {
                    auto* ev = reinterpret_cast<xcb_configure_notify_event_t*>(event.get());
                    if (ev->x != window_rect_.x || ev->y != window_rect_.y) {
                        window_rect_.x = ev->x, window_rect_.y = ev->y;
                        moving_timer_start_ = std::chrono::high_resolution_clock::now();
                        is_moving_ = true;
                    }
                    if (ev->width != window_rect_.width || ev->height != window_rect_.height) {
                        window_rect_.width = ev->width, window_rect_.height = ev->height;
                        sizing_timer_start_ = std::chrono::high_resolution_clock::now();
                        is_sizing_ = true;
                    }
                    if ((is_moving_ || is_sizing_) && !is_sizing_or_moving_) {
                        is_sizing_or_moving_ = true;
                        CALL_AND_CHECK_RESULT(onEvent(Event::ENTER_SIZING_OR_MOVING));
                    }
                } break;

                case XCB_KEY_PRESS: {
                    auto* ev = reinterpret_cast<xcb_key_press_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail < key_state_table_.size()) {
                        auto& key_state = key_state_table_[ev->detail];
                        if (!key_state.release) {
                            CALL_AND_CHECK_RESULT(onKeyEvent(static_cast<KeyCode>(ev->detail), true));
                        }
                        key_state.release = false;
                    }
                } break;

                case XCB_KEY_RELEASE: {
                    auto* ev = reinterpret_cast<xcb_key_release_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail < key_state_table_.size()) {
                        auto& key_state = key_state_table_[ev->detail];
                        check_key_timers_ = true;
                        key_state.release = true;
                        key_state.release_timer_start = std::chrono::high_resolution_clock::now();
                    }
                    break;
                }

                case XCB_BUTTON_PRESS: {
                    auto* ev = reinterpret_cast<xcb_button_press_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail <= 3) {
                        CALL_AND_CHECK_RESULT(
                            onMouseButtonEvent(static_cast<KeyCode>(ev->detail), true, ev->event_x, ev->event_y));
                    } else if (ev->detail == 4 || ev->detail == 5) {
                        CALL_AND_CHECK_RESULT(onMouseWheel(ev->detail == 4 ? 0.24f : -0.24f, ev->event_x, ev->event_y,
                                                           get_mouse_button_mask(ev->state)));
                    }
                } break;

                case XCB_BUTTON_RELEASE: {
                    auto* ev = reinterpret_cast<xcb_button_release_event_t*>(event.get());
                    if (ev->detail >= 1 && ev->detail <= 3) {
                        CALL_AND_CHECK_RESULT(
                            onMouseButtonEvent(static_cast<KeyCode>(ev->detail), false, ev->event_x, ev->event_y));
                    }
                } break;

                case XCB_MOTION_NOTIFY: {
                    auto* ev = reinterpret_cast<xcb_motion_notify_event_t*>(event.get());
                    CALL_AND_CHECK_RESULT(onMouseMove(ev->event_x, ev->event_y, get_mouse_button_mask(ev->state)));
                } break;
            }
        } else if (!is_sizing_) {
            CALL_AND_CHECK_RESULT(onIdle());
        }
    }
}

xcb_atom_t MainWindow::Implementation::queryAtom(std::string_view name) {
    std::unique_ptr<xcb_intern_atom_reply_t, void (*)(void*)> reply{
        ::xcb_intern_atom_reply(connection_,
                                ::xcb_intern_atom(connection_, 1, static_cast<std::uint16_t>(name.size()), name.data()),
                                nullptr),
        std::free};
    return reply->atom;
}

void MainWindow::Implementation::getPropertyValues(xcb_atom_t property, std::vector<xcb_atom_t>& values) {
    values.clear();
    for (std::uint32_t offset = 0;; ++offset) {
        xcb_get_property_cookie_t cookie = ::xcb_get_property(connection_, 0, window_, property, XCB_ATOM_ATOM, offset,
                                                              1);
        std::unique_ptr<xcb_get_property_reply_t, void (*)(void*)> reply{
            ::xcb_get_property_reply(connection_, cookie, nullptr), std::free};
        if (::xcb_get_property_value_length(reply.get()) != sizeof(xcb_atom_t)) { break; }
        values.push_back(*reinterpret_cast<xcb_atom_t*>(::xcb_get_property_value(reply.get())));
    }
}

// --------------------------------------------------------
// MainWindow class implementation

MainWindow::MainWindow() : impl_(std::make_unique<Implementation>(*this)) {}

MainWindow::~MainWindow() {}

rel::WindowDescriptor MainWindow::getWindowDescriptor() const {
    rel::WindowDescriptor win_desc{.platform = rel::PlatformType::PLATFORM_XCB};
    static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
    static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                  "Too little WindowDescriptor alignment");
    WindowDescImpl& win_desc_impl = *reinterpret_cast<WindowDescImpl*>(&win_desc.handle);
    win_desc_impl.connection = impl_->connection_;
    win_desc_impl.window = impl_->window_;
    return win_desc;
}

bool MainWindow::createWindow(const char* title, std::uint32_t width, std::uint32_t height) {
    return impl_->createWindow(title, width, height);
}

void MainWindow::showWindow() { return impl_->showWindow(); }

int MainWindow::mainLoop() { return impl_->mainLoop(); }
