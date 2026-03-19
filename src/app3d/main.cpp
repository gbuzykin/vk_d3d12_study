#include "main_window.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "util/range_helpers.h"

#include <uxs/db/json.h>
#include <uxs/io/filebuf.h>
#include <uxs/io/iostate.h>

#include <stb_image.h>

#include <array>
#include <chrono>
#include <exception>
#include <vector>

using namespace app3d;

namespace {
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t num_components = 0;
    std::vector<std::uint8_t> data;
};

bool loadImageFromFile(const char* filename, Image& image, std::uint32_t num_requested_components = 0) {
    int width = 0;
    int height = 0;
    int num_components = 0;
    std::unique_ptr<unsigned char, void (*)(void*)> stbi_data(
        stbi_load(filename, &width, &height, &num_components, num_requested_components), stbi_image_free);

    if (!stbi_data || width <= 0 || height <= 0 || num_components <= 0) {
        logError("error loading image '{}'", filename);
        return false;
    }

    image.width = width;
    image.height = height;
    image.num_components = num_components;
    const std::size_t data_size = std::size_t(width) * std::size_t(height) *
                                  (0 < num_requested_components ? num_requested_components : num_components);

    image.data.assign(stbi_data.get(), stbi_data.get() + data_size);
    return true;
}

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
}  // namespace

class App3DMainWindow final : public MainWindow {
 public:
    int init(int argc, char** argv);

    bool onIdle(int& ret_code) override {
        if (recreate_swap_chain_scheduled_) {
            const auto timer_now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double>(timer_now - recreate_swap_chain_timer_start_).count() > 0.25) {
                if (!surface_->createSwapChain(*device_, swap_chain_opts_)) {
                    ret_code = -1;
                    return false;
                }
                recreate_swap_chain_scheduled_ = false;
            } else {
                return true;
            }
        }

        if (!renderScene()) {
            ret_code = -1;
            return false;
        }

        const auto time_now = std::chrono::high_resolution_clock::now();
        ++frame_counter_;
        const double delta = std::chrono::duration<double>(time_now - time_fps_last_).count();
        if (delta > 2) {
            logInfo("fps = {}", frame_counter_ / delta);
            frame_counter_ = 0;
            time_fps_last_ = time_now;
        }
        return true;
    }

    bool onResize(int& ret_code) override {
        logInfo("resize");
        scheduleRecreateSwapChain();
        return true;
    }

    bool onKeyEvent(KeyCode key, bool state, int& ret_code) override {
        logInfo("key {} {}", key_text[unsigned(key)], state ? "pressed" : "unpressed");
        return true;
    }

    void onMouseButtonEvent(KeyCode button, bool state, std::int32_t x, std::int32_t y) override {
        logInfo("mouse button {} {}, x = {}, y = {}", unsigned(button), state ? "pressed" : "unpressed", x, y);
    }

    void onMouseWheel(float distance, std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        logInfo("mouse wheel {}, x = {}, y = {}, mask = {}", distance, x, y, button_mask);
    }

    void onMouseMove(std::int32_t x, std::int32_t y, std::uint8_t button_mask) override {
        //   logInfo("mouse move, x = {}, y = {}, mask = {}", x, y, button_mask);
    }

 private:
    std::uint64_t frame_counter_ = 0;
    std::chrono::high_resolution_clock::time_point time_fps_last_{};
    std::chrono::high_resolution_clock::time_point recreate_swap_chain_timer_start_{};
    bool recreate_swap_chain_scheduled_ = false;

    std::unique_ptr<rel::IRenderingDriver> driver_;
    rel::ISurface* surface_ = nullptr;

    uxs::db::value device_caps_;
    rel::IDevice* device_ = nullptr;

    uxs::db::value swap_chain_opts_;
    rel::ISwapChain* swap_chain_ = nullptr;

    rel::IRenderTarget* render_target_ = nullptr;
    rel::IPipeline* pipeline_ = nullptr;
    rel::ITexture* texture_ = nullptr;
    rel::ISampler* sampler_ = nullptr;
    rel::IDescriptorSet* descriptor_set_ = nullptr;
    rel::IBuffer* vertex_buffer_ = nullptr;

    void scheduleRecreateSwapChain() {
        recreate_swap_chain_timer_start_ = std::chrono::high_resolution_clock::now();
        recreate_swap_chain_scheduled_ = true;
    }

    bool initScene();
    bool renderScene();
};

#define JSON(...) uxs::db::json::read_from_string(#__VA_ARGS__)

int App3DMainWindow::init(int argc, char** argv) {
    void* driver_library = loadDynamicLibrary(".", "app3d-rel-vulkan");
    if (!driver_library) { return -1; }

    auto* entry = (rel::GetDriverDescriptorFuncPtr)getDynamicLibraryEntry(driver_library,
                                                                          "app3dGetRenderingDriverDescriptor");
    if (!entry) { return -1; }

    const auto app_info = JSON({"name" : "App3D", "version" : [ 1, 0, 0 ]});

    if (!(driver_ = entry()->create_func()) || !driver_->init(app_info)) { return -1; }

    if (!createWindow(app_info.value<std::string>("name"), 1280, 1024)) { return -1; }

    if (!(surface_ = driver_->createSurface(getWindowDescriptor()))) { return -1; }

    std::uint32_t device_index = 0;
    std::uint32_t device_count = driver_->getPhysicalDeviceCount();

    device_caps_ = JSON({"needs_compute" : true});

    for (device_index = 0; device_index < device_count; ++device_index) {
        if (driver_->isSuitablePhysicalDevice(device_index, device_caps_)) { break; }
    }

    if (device_index == device_count) {
        logError("no suitable physical device");
        return -1;
    }

    if (!(device_ = driver_->createDevice(device_index, device_caps_))) { return -1; }

    if (!(swap_chain_ = surface_->createSwapChain(*device_, swap_chain_opts_))) { return -1; }

    if (!(render_target_ = swap_chain_->createRenderTarget({}))) { return -1; }

    if (!initScene()) { return -1; }

    showWindow();
    time_fps_last_ = std::chrono::high_resolution_clock::now();
    return 0;
}

bool App3DMainWindow::initScene() {
    std::vector<std::uint32_t> vertex_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/sampler/vert.spv", "r"); ifile) {
        vertex_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(vertex_shader_spv));
    }

    std::vector<std::uint32_t> pixel_shader_spv;
    if (uxs::bfilebuf ifile("data/shaders/sampler/pix.spv", "r"); ifile) {
        pixel_shader_spv.resize(ifile.seek(0, uxs::seekdir::end) / sizeof(std::uint32_t));
        ifile.seek(0);
        ifile.read(util::as_byte_span(pixel_shader_spv));
    }

    auto* vertex_shader_module = device_->createShaderModule(vertex_shader_spv);
    if (!vertex_shader_module) { return false; }

    auto* pixel_shader_module = device_->createShaderModule(pixel_shader_spv);
    if (!pixel_shader_module) { return false; }

    const auto pipeline = JSON({
        "stages" : [
            {"stage" : "vertex", "module_index" : 0, "entry" : "main"},
            {"stage" : "pixel", "module_index" : 1, "entry" : "main"}
        ],
        "vertex_layouts" : [ {
            "binding" : 0,
            "stride" : 20,
            "attributes" : {"0" : {"format" : "float3", "offset" : 0}, "1" : {"format" : "float2", "offset" : 12}}
        } ]
    });

    if (!(pipeline_ = device_->createPipeline(*render_target_, std::array{vertex_shader_module, pixel_shader_module},
                                              pipeline))) {
        return false;
    }

    Image image;
    if (!loadImageFromFile("data/images/sunset.jpg", image, 4)) { return false; }

    rel::Extent3u image_extent{.width = image.width, .height = image.height, .depth = 1};

    if (!(texture_ = device_->createTexture(image_extent))) { return false; }

    if (!texture_->updateTexture(image.data, {}, image_extent)) { return false; }

    if (!(sampler_ = device_->createSampler())) { return false; }

    if (!(descriptor_set_ = device_->createDescriptorSet(*pipeline_))) { return false; }
    descriptor_set_->updateTextureSamplerDescriptor(*texture_, *sampler_);

    const std::vector<float> vertices{
        -0.75f, -0.75f, 0.0f, 0.0f, 0.0f, -0.75f, 0.75f, 0.0f, 0.0f, 1.0f,
        0.75f,  -0.75f, 0.0f, 1.0f, 0.0f, 0.75f,  0.75f, 0.0f, 1.0f, 1.0f,
    };

    if (!(vertex_buffer_ = device_->createBuffer(sizeof(vertices[0]) * vertices.size()))) { return false; }

    if (!vertex_buffer_->updateBuffer(util::as_byte_span(vertices), 0)) { return false; }

    return true;
}

bool App3DMainWindow::renderScene() {
    const auto result = render_target_->beginRenderTarget({0.1f, 0.2f, 0.3f, 1.0f});
    if (result == rel::RenderTargetResult::SUBOPTIMAL || result == rel::RenderTargetResult::OUT_OF_DATE) {
        scheduleRecreateSwapChain();
        if (result == rel::RenderTargetResult::OUT_OF_DATE) { return true; }
    } else if (result != rel::RenderTargetResult::SUCCESS) {
        return false;
    }

    render_target_->bindPipeline(*pipeline_);

    render_target_->setViewport(rel::Rect{.extent = swap_chain_->getImageExtent()}, 0.0f, 1.0f);

    render_target_->setScissor(rel::Rect{.extent = swap_chain_->getImageExtent()});

    render_target_->bindDescriptorSet(*pipeline_, *descriptor_set_, 0);

    render_target_->bindVertexBuffer(*vertex_buffer_, 0, 0);

    render_target_->drawGeometry(4, 1, 0, 0);

    if (!render_target_->endRenderTarget()) { return false; }

    return true;
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
