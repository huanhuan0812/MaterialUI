// src/ui/application.hpp
#pragma once

#include "../native/event/EventLoop.hpp"
#include "../window/window.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ui {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // ---------- 窗口管理 ----------
    Window* createWindow(const std::string& title, int w, int h);
    void    destroyWindow(Window* w);   // 延迟销毁

    // ---------- 生命周期 ----------
    int  run();
    void quit();

    // ---------- 访问 ----------
    EventLoop& loop() { return *loop_; }
    bool isRunning() const { return running_; }

private:
    // Window 析构/关闭时回调，用于从 windows_ 中移除并解绑
    void onWindowClosed(Window* w);
    friend class Window;   // 允许 Window 调 onWindowClosed

    void flushDestroy();
    void flushWindows();

    std::unique_ptr<EventLoop>            loop_;
    std::vector<std::unique_ptr<Window>>  windows_;
    std::vector<Window*>                  pendingDestroy_;
    bool                                  running_ = false;
    bool                                  shouldQuit_ = false;
};

} // namespace ui