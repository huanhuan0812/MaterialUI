#pragma once

#include "NativeWidget.h"

#if defined(__linux__)
#pragma once

#include "NativeWidget.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace ui {

class LinuxX11NativeWidget final : public NativeWidget {
public:
    LinuxX11NativeWidget();
    ~LinuxX11NativeWidget() override;

    bool create(const std::string& title, int w, int h) override;
    void show() override;
    void hide() override;
    void close() override;

    bool shouldQuit() const override { return shouldQuit_; }

    void setTitle(const std::string& title) override;
    void setSize(int w, int h) override;
    void getSize(int& w, int& h) const override;

    void invalidate(const Rect& r) override;
    void invalidateAll() override;
    void repaintNow() override;

    void setPaintCallback(PaintCallback cb) override;
    void setEventCallback(EventCallback cb) override;

private:
    void processEvent(XEvent& ev);
    void doPaint(const Rect& dirty);
    void translateKey(XKeyEvent& kev, Event& out);

    Display*  dpy_         = nullptr;
    Window    win_         = 0;
    GC        gc_          = 0;
    Atom      wmDelete_    = 0;
    Atom      wmProtocols_ = 0;
    Visual*   visual_      = nullptr;
    int       screen_      = 0;
    int       depth_       = 24;

    int       width_       = 0;
    int       height_      = 0;

    bool      running_     = false;
    bool      visible_     = false;
    bool      dirty_       = false;
    Rect      dirtyRect_{};

    std::atomic<bool> needsRepaint_{false};
    std::mutex        paintMutex_;

    PaintCallback paintCb_;
    EventCallback eventCb_;
};

// 由 NativeWidget.cpp 调用
std::unique_ptr<NativeWidget> createX11Widget();

} // namespace ui
#endif // __linux__