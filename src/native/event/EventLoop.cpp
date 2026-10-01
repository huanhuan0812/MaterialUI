// src/native/event/EventLoop.cpp
#include "EventLoop.hpp"

#if defined(_WIN32)
namespace ui { std::unique_ptr<EventLoop> createWin32EventLoop(); }
#elif defined(__APPLE__)
namespace ui { std::unique_ptr<EventLoop> createCocoaEventLoop(); }
#elif defined(__linux__)
namespace ui {
    std::unique_ptr<EventLoop> createX11EventLoop();
    std::unique_ptr<EventLoop> createWaylandEventLoop();
}
#  include <cstdlib>
#  include <cstring>
#endif

namespace ui {

#if defined(__linux__)

static bool preferWayland() {
    const char* wl = std::getenv("WAYLAND_DISPLAY");
    if (wl && *wl) return true;
    const char* st = std::getenv("XDG_SESSION_TYPE");
    return st && std::strcmp(st, "wayland") == 0;
}

static bool hasX11() {
    const char* x = std::getenv("DISPLAY");
    return x && *x;
}

#endif

std::unique_ptr<EventLoop> EventLoop::create() {
#if defined(_WIN32)
    return createWin32EventLoop();
#elif defined(__APPLE__)
    return createCocoaEventLoop();
#elif defined(__linux__)
    if (preferWayland()) {
        auto loop = createWaylandEventLoop();
        if (loop) return loop;
        if (hasX11()) return createX11EventLoop();
        return nullptr;
    }
    if (hasX11()) {
        auto loop = createX11EventLoop();
        if (loop) return loop;
        return createWaylandEventLoop();
    }
    return createWaylandEventLoop();
#else
    return nullptr;
#endif
}

} // namespace ui