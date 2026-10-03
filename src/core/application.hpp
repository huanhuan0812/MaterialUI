#pragma once
#include "core/handle_pool.hpp"
#include "component/widget.hpp"
#include "window/window.hpp"
#include "native/event/EventLoop.hpp"
#include <memory>
#include <vector>

namespace ui {

class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    template <typename T, typename... Args>
    Handle createWidget(Args&&... args) {
        auto w = std::make_shared<T>(std::forward<Args>(args)...);
        Handle h = widgets_.add(w);
        w->attach(h, &widgets_);
        return h;
    }

    std::shared_ptr<Widget> widget(Handle h) const {
        return widgets_.get(h);
    }
    void destroyWidget(Handle h) { widgets_.remove(h); }

    Window* createWindow(const std::string& title, int w, int h);
    void    destroyWindow(Window* w);

    int  run();
    void quit();

    EventLoop& loop() { return *loop_; }
    bool isRunning() const { return running_; }

private:
    void onWindowClosed(Window* w);
    void flushDestroy();

    std::unique_ptr<EventLoop> loop_;
    HandlePool<Widget>         widgets_;
    std::vector<std::unique_ptr<Window>> windows_;
    std::vector<Window*>                 pendingDestroy_;
    bool running_ = false;
    bool shouldQuit_ = false;
};

} // namespace ui