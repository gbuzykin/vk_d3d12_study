#include "main_window.h"

#include "utils/print.h"

using namespace app3d;

class App3DMainWindow : public MainWindow {
 public:
    int init(int argc, char** argv);
};

int App3DMainWindow::init(int argc, char** argv) {
    std::string app_name{"App3D"};
    if (!createWindow(app_name, 1024, 768)) { return -1; }
    showWindow();
    return 0;
}

int main(int argc, char** argv) {
    App3DMainWindow win;

    int init_result = win.init(argc, argv);
    if (init_result != 0) { return init_result; }

    return win.mainLoop();
}
