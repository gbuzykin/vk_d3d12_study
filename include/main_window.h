#pragma once

#include "window_descriptor.h"

#include <memory>
#include <string>

namespace app3d {

struct WindowDescriptor;

class MainWindow {
 public:
    MainWindow();
    virtual ~MainWindow();

    WindowDescriptor getWindowDescriptor() const;

    bool createWindow(const std::string& window_title, int width, int height);
    void showWindow();
    int mainLoop();

    virtual bool render(int& ret_code) { return false; }

 private:
    struct ImplData;
    std::unique_ptr<ImplData> impl_data_;
};

}  // namespace app3d
