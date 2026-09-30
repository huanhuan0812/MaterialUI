// src/native/native_widget/NativeWidget.cpp
#include "NativeWidget.h"

#if defined(_WIN32)
namespace ui { std::unique_ptr<Widget> createWin32Widget(); }
#elif defined(__APPLE__)
namespace ui { std::unique_ptr<Widget> createCocoaWidget(); }
#elif defined(__linux__)
namespace ui {
    std::unique_ptr<Widget> createX11Widget();
    std::unique_ptr<Widget> createWaylandWidget();
}
#  include <cstdlib>
#  include <cstring>
#endif

namespace ui {

#if defined(__linux__)

// 优先 Wayland：现代桌面（GNOME/KDE/Sway）优先；
// 若不在 Wayland 会话再退回 X11（含 XWayland）
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

std::unique_ptr<Widget> Widget::create() {
#if defined(_WIN32)
    return createWin32Widget();
#elif defined(__APPLE__)
    return createCocoaWidget();
#elif defined(__linux__)
    if (preferWayland()) {
        auto w = createWaylandWidget();
        if (w) return w;
        // Wayland 初始化失败时回退到 X11
        if (hasX11()) return createX11Widget();
        return nullptr;
    }
    if (hasX11()) {
        auto w = createX11Widget();
        if (w) return w;
        // X11 失败则尝试 Wayland
        return createWaylandWidget();
    }
    // 都没有：尝试 Wayland（例如会话环境变量被清除但 socket 存在）
    return createWaylandWidget();
#else
    return nullptr;
#endif
}

} // namespace ui