#include "main_window.h"

#include "common/logger.h"

#include <exception>

using namespace app3d;

class App3DMainWindow final : public MainWindow {
 public:
    int init(int argc, char** argv);
};

int App3DMainWindow::init(int argc, char** argv) {
    std::string app_name{"App3D"};
    if (!createWindow(app_name, 1280, 1024)) { return -1; }
    showWindow();
    return 0;
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
