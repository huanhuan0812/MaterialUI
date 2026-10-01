#include "linuxX11NativeWidget.h"

#if defined(__linux__)
#include <algorithm>
#include <cstring>
#include <poll.h>
#include <unistd.h>

namespace ui {

LinuxX11NativeWidget::LinuxX11NativeWidget()  = default;
LinuxX11NativeWidget::~LinuxX11NativeWidget() { close(); }

// ------------------------------------------------------------------
// 生命周期
// ------------------------------------------------------------------
bool LinuxX11NativeWidget::create(const std::string& title, int w, int h) {
    if (dpy_) return true;

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) return false;

    screen_ = DefaultScreen(dpy_);
    visual_ = DefaultVisual(dpy_, screen_);
    depth_  = DefaultDepth(dpy_, screen_);
    width_  = w;
    height_ = h;

    Window root = RootWindow(dpy_, screen_);

    XSetWindowAttributes swa{};
    swa.background_pixel = BlackPixel(dpy_, screen_);
    swa.event_mask = ExposureMask | StructureNotifyMask |
                     KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                     FocusChangeMask;

    win_ = XCreateWindow(dpy_, root,
                         0, 0, w, h, 0,
                         depth_, InputOutput, visual_,
                         CWBackPixel | CWEventMask, &swa);
    if (!win_) { XCloseDisplay(dpy_); dpy_ = nullptr; return false; }

    XStoreName(dpy_, win_, title.c_str());

    wmDelete_    = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    wmProtocols_ = XInternAtom(dpy_, "WM_PROTOCOLS", False);
    XSetWMProtocols(dpy_, win_, &wmDelete_, 1);

    gc_ = XCreateGC(dpy_, win_, 0, nullptr);

    running_ = true;
    return true;
}

void LinuxX11NativeWidget::show() {
    if (!dpy_ || !win_) return;
    XMapWindow(dpy_, win_);
    XFlush(dpy_);
    visible_ = true;
}

void LinuxX11NativeWidget::hide() {
    if (!dpy_ || !win_) return;
    XUnmapWindow(dpy_, win_);
    XFlush(dpy_);
    visible_ = false;
}

void LinuxX11NativeWidget::close() {
    running_ = false;
    if (dpy_) {
        if (gc_)  { XFreeGC(dpy_, gc_);        gc_  = 0; }
        if (win_) { XDestroyWindow(dpy_, win_); win_ = 0; }
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
    }
    visible_ = false;
}

// ------------------------------------------------------------------
// 主循环
// ------------------------------------------------------------------
bool LinuxX11NativeWidget::pumpEvents(int timeoutMs) {
    if (!dpy_ || !running_) return false;

    if (needsRepaint_.exchange(false)) {
        Rect r;
        {
            std::lock_guard<std::mutex> lk(paintMutex_);
            r = dirty_ ? dirtyRect_ : Rect{0, 0, width_, height_};
            dirty_      = false;
            dirtyRect_  = Rect{};
        }
        doPaint(r);
    }

    bool pending = XPending(dpy_) > 0;

    if (!pending) {
        if (timeoutMs == 0) return running_;

        struct pollfd pfd{};
        pfd.fd     = ConnectionNumber(dpy_);
        pfd.events = POLLIN;
        int to     = (timeoutMs < 0) ? -1 : timeoutMs;
        int ret    = ::poll(&pfd, 1, to);
        if (ret <= 0) return running_;
    }

    while (XPending(dpy_)) {
        XEvent ev;
        XNextEvent(dpy_, &ev);
        processEvent(ev);
        if (!running_) return false;
    }
    return running_;
}

// ------------------------------------------------------------------
// 事件
// ------------------------------------------------------------------
void LinuxX11NativeWidget::processEvent(XEvent& ev) {
    switch (ev.type) {
    case Expose: {
        const XExposeEvent& e = ev.xexpose;
        if (e.count == 0)
            invalidate(Rect{e.x, e.y, e.width, e.height});
        break;
    }
    case ConfigureNotify: {
        const XConfigureEvent& e = ev.xconfigure;
        width_  = e.width;
        height_ = e.height;
        if (eventCb_) {
            Event evt{};
            evt.type          = EventType::Resize;
            evt.resize.width  = e.width;
            evt.resize.height = e.height;
            eventCb_(evt);
        }
        invalidateAll();
        break;
    }
    case ClientMessage: {
        const XClientMessageEvent& e = ev.xclient;
        if (static_cast<Atom>(e.data.l[0]) == wmDelete_) {
            running_ = false;
            if (eventCb_) { Event evt{}; evt.type = EventType::Close; eventCb_(evt); }
        }
        break;
    }
    case KeyPress: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::KeyDown;
        translateKey(ev.xkey, evt);
        eventCb_(evt);
        break;
    }
    case KeyRelease: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::KeyUp;
        translateKey(ev.xkey, evt);
        eventCb_(evt);
        break;
    }
    case ButtonPress: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::MouseDown;
        evt.mouse.x = ev.xbutton.x;
        evt.mouse.y = ev.xbutton.y;
        evt.mouse.button = ev.xbutton.button;
        eventCb_(evt);
        break;
    }
    case ButtonRelease: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::MouseUp;
        evt.mouse.x = ev.xbutton.x;
        evt.mouse.y = ev.xbutton.y;
        evt.mouse.button = ev.xbutton.button;
        eventCb_(evt);
        break;
    }
    case MotionNotify: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::MouseMove;
        evt.mouse.x = ev.xmotion.x;
        evt.mouse.y = ev.xmotion.y;
        eventCb_(evt);
        break;
    }
    case EnterNotify: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::MouseEnter;
        evt.mouse.x = ev.xcrossing.x;
        evt.mouse.y = ev.xcrossing.y;
        eventCb_(evt);
        break;
    }
    case LeaveNotify: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::MouseLeave;
        evt.mouse.x = ev.xcrossing.x;
        evt.mouse.y = ev.xcrossing.y;
        eventCb_(evt);
        break;
    }
    case FocusIn: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::FocusIn;
        eventCb_(evt);
        break;
    }
    case FocusOut: {
        if (!eventCb_) break;
        Event evt{}; evt.type = EventType::FocusOut;
        eventCb_(evt);
        break;
    }
    default: break;
    }
}

void LinuxX11NativeWidget::translateKey(XKeyEvent& kev, Event& out) {
    KeySym ks = XLookupKeysym(&kev, 0);
    out.key.keycode = static_cast<int>(ks);
    out.key.ch = (ks >= XK_space && ks <= XK_asciitilde)
                     ? static_cast<char>(ks) : 0;
    out.key.ctrl  = (kev.state & ControlMask) != 0;
    out.key.shift = (kev.state & ShiftMask)   != 0;
    out.key.alt   = (kev.state & Mod1Mask)    != 0;
}

// ------------------------------------------------------------------
// 属性
// ------------------------------------------------------------------
void LinuxX11NativeWidget::setTitle(const std::string& title) {
    if (!dpy_ || !win_) return;
    XStoreName(dpy_, win_, title.c_str());
    XFlush(dpy_);
}

void LinuxX11NativeWidget::setSize(int w, int h) {
    if (!dpy_ || !win_) return;
    width_  = w;
    height_ = h;
    XResizeWindow(dpy_, win_, w, h);
    XFlush(dpy_);
}

void LinuxX11NativeWidget::getSize(int& w, int& h) const {
    w = width_;
    h = height_;
}

// ------------------------------------------------------------------
// 脏矩形
// ------------------------------------------------------------------
void LinuxX11NativeWidget::invalidate(const Rect& r) {
    std::lock_guard<std::mutex> lk(paintMutex_);
    if (!dirty_) {
        dirtyRect_ = r;
        dirty_     = true;
    } else {
        int x0 = std::min(dirtyRect_.x, r.x);
        int y0 = std::min(dirtyRect_.y, r.y);
        int x1 = std::max(dirtyRect_.x + dirtyRect_.w, r.x + r.w);
        int y1 = std::max(dirtyRect_.y + dirtyRect_.h, r.y + r.h);
        dirtyRect_ = Rect{x0, y0, x1 - x0, y1 - y0};
    }
    needsRepaint_.store(true);
}

void LinuxX11NativeWidget::invalidateAll() {
    invalidate(Rect{0, 0, width_, height_});
}

void LinuxX11NativeWidget::repaintNow() {
    Rect r;
    {
        std::lock_guard<std::mutex> lk(paintMutex_);
        r = dirty_ ? dirtyRect_ : Rect{0, 0, width_, height_};
        dirty_     = false;
        dirtyRect_ = Rect{};
    }
    needsRepaint_.store(false);
    doPaint(r);
}

// ------------------------------------------------------------------
// 绘制
// ------------------------------------------------------------------
void LinuxX11NativeWidget::doPaint(const Rect& dirty) {
    if (!dpy_ || !win_) return;

    cairo_surface_t* surface =
        cairo_xlib_surface_create(dpy_, win_, visual_, width_, height_);
    cairo_t* cr = cairo_create(surface);

    Canvas canvas(cr, width_, height_);

    if (paintCb_) {
        cairo_save(cr);
        cairo_rectangle(cr, dirty.x, dirty.y, dirty.w, dirty.h);
        cairo_clip(cr);
        paintCb_(canvas, dirty);
        cairo_restore(cr);
    }

    cairo_destroy(cr);
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);

    XFlush(dpy_);
}

// ------------------------------------------------------------------
// 回调
// ------------------------------------------------------------------
void LinuxX11NativeWidget::setPaintCallback(PaintCallback cb) { paintCb_ = std::move(cb); }
void LinuxX11NativeWidget::setEventCallback(EventCallback cb) { eventCb_ = std::move(cb); }

// ------------------------------------------------------------------
// 工厂
// ------------------------------------------------------------------
std::unique_ptr<NativeWidget> createX11Widget() {
    auto w = std::make_unique<LinuxX11NativeWidget>();
    if (!w->create("UI", 800, 600)) return nullptr;
    return w;
}

} // namespace ui
#endif // __linux__