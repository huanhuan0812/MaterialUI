// src/native/event/wayland_event_loop.cpp
#include "EventLoop.hpp"
#include "../NativeWidget/NativeWidget.h"

#if defined(__linux__)
#include <memory>
#include <poll.h>

// 伪代码：实际依赖 wayland-client.h
struct wl_display;

namespace ui {
namespace {

class WaylandEventLoop : public EventLoop {
public:
    void attach(NativeWidget* widget) override {
        widget_ = widget;
    }

    int run() override {
        while (!shouldQuit()) {
            if (!step(-1)) break;
        }
        return 0;
    }

    void quit() override {
        quit_ = true;
    }

    bool step(int timeoutMs) override {
        if (shouldQuit()) return false;

        if (!display_) return false;

        // 1. 先派发已排队事件
        if (wl_display_prepare_read(display_) == 0) {
            wl_display_flush(display_);

            pollfd pfd{};
            pfd.fd = wl_display_get_fd(display_);
            pfd.events = POLLIN;

            int r = poll(&pfd, 1, timeoutMs);
            if (r > 0 && (pfd.revents & POLLIN)) {
                wl_display_read_events(display_);
            } else {
                wl_display_cancel_read(display_);
            }
        }

        wl_display_dispatch_pending(display_);
        return !shouldQuit();
    }

    void setDisplay(wl_display* d) { display_ = d; }

private:
    bool shouldQuit() const {
        if (quit_) return true;
        if (widget_ && widget_->shouldQuit()) return true;
        return false;
    }

    NativeWidget* widget_ = nullptr;
    wl_display* display_ = nullptr;
    bool quit_ = false;
};

} // namespace

std::unique_ptr<EventLoop> createWaylandEventLoop() {
    return std::make_unique<WaylandEventLoop>();
}

} // namespace ui
#endif