// src/native/event/x11_event_loop.cpp
#include "EventLoop.hpp"
#include "../NativeWidget/NativeWidget.h"

#if defined(__linux__) && !defined(__WAYLAND_ONLY__)
#include <X11/Xlib.h>
#include <memory>

namespace ui {
namespace {

class X11EventLoop : public EventLoop {
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

        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) return false;

        int fd = ConnectionNumber(dpy);

        if (XPending(dpy) == 0) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            timeval tv{};
            timeval* ptv = nullptr;
            if (timeoutMs >= 0) {
                tv.tv_sec = timeoutMs / 1000;
                tv.tv_usec = (timeoutMs % 1000) * 1000;
                ptv = &tv;
            }

            int r = select(fd + 1, &fds, nullptr, nullptr, ptv);
            if (r < 0) {
                XCloseDisplay(dpy);
                return false;
            }
        }

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            // 事件由 X11Widget 内部注册的处理器处理；
            // 这里只负责泵，不做具体分发。
        }

        XCloseDisplay(dpy);
        return !shouldQuit();
    }

private:
    bool shouldQuit() const {
        if (quit_) return true;
        if (widget_ && widget_->shouldQuit()) return true;
        return false;
    }

    NativeWidget* widget_ = nullptr;
    bool quit_ = false;
};

} // namespace

std::unique_ptr<EventLoop> createX11EventLoop() {
    return std::make_unique<X11EventLoop>();
}

} // namespace ui
#endif