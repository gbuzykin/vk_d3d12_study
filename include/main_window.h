#pragma once

#include <memory>
#include <string>

namespace app3d {

class MainWindow {
 public:
    MainWindow();
    virtual ~MainWindow();

    bool createWindow(const std::string& window_title, int width, int height);
    void showWindow();
    int mainLoop();

    virtual bool render(int& ret_code) { return false; }

 private:
    struct ImplData;
    std::unique_ptr<ImplData> impl_data_;
};

}  // namespace app3d
