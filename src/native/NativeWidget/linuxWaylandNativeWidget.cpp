#include "linuxWaylandNativeWidget.h"

#if defined(__linux__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <linux/input-event-codes.h>

namespace ui {

LinuxWaylandNativeWidget::LinuxWaylandNativeWidget()  = default;
LinuxWaylandNativeWidget::~LinuxWaylandNativeWidget() { close(); }

// ------------------------------------------------------------------
// SHM 工具
// ------------------------------------------------------------------
static int createShmFile(size_t size) {
    char name[] = "/tmp/wayland-shm-XXXXXX";
    int fd = mkstemp(name);
    if (fd < 0) return -1;
    unlink(name);
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

bool LinuxWaylandNativeWidget::createShmBuffer(ShmBuffer& buf, int w, int h) {
    int    stride = w * 4;
    size_t size   = static_cast<size_t>(stride) * h;

    int fd = createShmFile(size);
    if (fd < 0) return false;

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { ::close(fd); return false; }

    wl_shm_pool* pool = wl_shm_create_pool(shm_, fd, static_cast<int>(size));
    buf.buffer = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
                                           WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    ::close(fd);

    buf.data = data;
    buf.size = size;
    buf.w    = w;
    buf.h    = h;
    buf.busy = false;
    return true;
}

void LinuxWaylandNativeWidget::destroyBuffer(ShmBuffer& buf) {
    if (buf.buffer) { wl_buffer_destroy(buf.buffer); buf.buffer = nullptr; }
    if (buf.data)   { munmap(buf.data, buf.size);    buf.data   = nullptr; }
    buf.size = 0;
}

LinuxWaylandNativeWidget::ShmBuffer* LinuxWaylandNativeWidget::nextFreeBuffer() {
    for (auto& b : buffers_)
        if (!b.busy) return &b;
    return nullptr;
}

void LinuxWaylandNativeWidget::ensureBuffers(int w, int h) {
    bool needRebuild = buffers_.empty();
    if (!needRebuild) {
        for (auto& b : buffers_)
            if (b.w != w || b.h != h) { needRebuild = true; break; }
    }
    if (!needRebuild) return;

    for (auto& b : buffers_) destroyBuffer(b);
    buffers_.clear();

    buffers_.resize(2);
    for (auto& b : buffers_) {
        if (!createShmBuffer(b, w, h)) { buffers_.clear(); return; }
    }
}

// ------------------------------------------------------------------
// 生命周期
// ------------------------------------------------------------------
bool LinuxWaylandNativeWidget::create(const std::string& title, int w, int h) {
    if (display_) return true;

    title_  = title;
    width_  = w;
    height_ = h;

    display_ = wl_display_connect(nullptr);
    if (!display_) return false;

    registry_ = wl_display_get_registry(display_);
    static const wl_registry_listener regListener = {
        registryGlobal, registryGlobalRemove
    };
    wl_registry_add_listener(registry_, &regListener, this);

    wl_display_roundtrip(display_);

    if (!compositor_ || !shm_ || !wmBase_) { close(); return false; }

    wl_display_roundtrip(display_);

    surface_    = wl_compositor_create_surface(compositor_);
    xdgSurface_ = xdg_wm_base_get_xdg_surface(wmBase_, surface_);

    static const xdg_surface_listener xdgSurfListener = { xdgSurfaceConfigure };
    xdg_surface_add_listener(xdgSurface_, &xdgSurfListener, this);

    xdgToplevel_ = xdg_surface_get_toplevel(xdgSurface_);

    static const xdg_toplevel_listener tlListener = {
        toplevelConfigure,
        toplevelClose,
        toplevelConfigureBounds,
        toplevelWmCapabilities
    };
    xdg_toplevel_add_listener(xdgToplevel_, &tlListener, this);

    xdg_toplevel_set_title(xdgToplevel_, title.c_str());
    xdg_toplevel_set_app_id(xdgToplevel_, "ui.native.widget");

    wl_surface_commit(surface_);

    running_ = true;
    return true;
}

void LinuxWaylandNativeWidget::show() {
    if (!surface_) return;
    visible_ = true;
    if (configured_) {
        ensureBuffers(width_, height_);
        doPaint();
    }
}

void LinuxWaylandNativeWidget::hide() {
    if (!surface_) return;
    wl_surface_attach(surface_, nullptr, 0, 0);
    wl_surface_commit(surface_);
    visible_ = false;
}

void LinuxWaylandNativeWidget::close() {
    running_ = false;

    for (auto& b : buffers_) destroyBuffer(b);
    buffers_.clear();

    if (xdgToplevel_) { xdg_toplevel_destroy(xdgToplevel_); xdgToplevel_ = nullptr; }
    if (xdgSurface_)  { xdg_surface_destroy(xdgSurface_);   xdgSurface_  = nullptr; }
    if (surface_)     { wl_surface_destroy(surface_);       surface_     = nullptr; }

    if (keyboard_) { wl_keyboard_destroy(keyboard_); keyboard_ = nullptr; }
    if (pointer_)  { wl_pointer_destroy(pointer_);   pointer_  = nullptr; }
    if (seat_)     { wl_seat_destroy(seat_);         seat_     = nullptr; }

    if (wmBase_)     { xdg_wm_base_destroy(wmBase_);       wmBase_     = nullptr; }
    if (shm_)        { wl_shm_destroy(shm_);               shm_        = nullptr; }
    if (compositor_) { wl_compositor_destroy(compositor_); compositor_ = nullptr; }

    if (registry_) { wl_registry_destroy(registry_); registry_ = nullptr; }
    if (display_)  { wl_display_disconnect(display_); display_ = nullptr; }

    visible_    = false;
    configured_ = false;
}

// ------------------------------------------------------------------
// 主循环
// ------------------------------------------------------------------
bool LinuxWaylandNativeWidget::pumpEvents(int timeoutMs) {
    if (!display_ || !running_) return false;

    if (needsRepaint_.exchange(false)) {
        ensureBuffers(width_, height_);
        doPaint();
    }

    while (wl_display_prepare_read(display_) != 0) {
        if (wl_display_dispatch_pending(display_) < 0) {
            running_ = false;
            return false;
        }
    }

    wl_display_flush(display_);

    struct pollfd pfd{};
    pfd.fd     = wl_display_get_fd(display_);
    pfd.events = POLLIN;

    int to  = (timeoutMs < 0) ? -1 : timeoutMs;
    int ret = ::poll(&pfd, 1, to);

    if (ret > 0 && (pfd.revents & POLLIN)) {
        if (wl_display_read_events(display_) < 0) {
            running_ = false;
            return false;
        }
    } else {
        wl_display_cancel_read(display_);
    }

    if (wl_display_dispatch_pending(display_) < 0) {
        running_ = false;
        return false;
    }

    if (needsRepaint_.exchange(false)) {
        ensureBuffers(width_, height_);
        doPaint();
    }

    return running_;
}

// ------------------------------------------------------------------
// 属性
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::setTitle(const std::string& title) {
    title_ = title;
    if (xdgToplevel_) {
        xdg_toplevel_set_title(xdgToplevel_, title.c_str());
        wl_surface_commit(surface_);
    }
}

void LinuxWaylandNativeWidget::setSize(int w, int h) {
    width_  = w;
    height_ = h;
    if (configured_) {
        ensureBuffers(w, h);
        invalidateAll();
    }
}

void LinuxWaylandNativeWidget::getSize(int& w, int& h) const {
    w = width_;
    h = height_;
}

// ------------------------------------------------------------------
// 脏矩形
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::invalidate(const Rect& r) {
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

void LinuxWaylandNativeWidget::invalidateAll() {
    invalidate(Rect{0, 0, width_, height_});
}

void LinuxWaylandNativeWidget::repaintNow() {
    {
        std::lock_guard<std::mutex> lk(paintMutex_);
        dirty_     = false;
        dirtyRect_ = Rect{};
    }
    needsRepaint_.store(false);
    ensureBuffers(width_, height_);
    doPaint();
}

// ------------------------------------------------------------------
// 绘制
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::doPaint() {
    if (!surface_ || !configured_) return;

    ShmBuffer* buf = nextFreeBuffer();
    if (!buf || buf->w != width_ || buf->h != height_) return;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(buf->data),
        CAIRO_FORMAT_ARGB32, buf->w, buf->h, buf->w * 4);
    cairo_t* cr = cairo_create(surface);

    Canvas canvas(cr, buf->w, buf->h);

    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    Rect dirty{0, 0, buf->w, buf->h};
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

    wl_surface_attach(surface_, buf->buffer, 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, buf->w, buf->h);
    wl_surface_commit(surface_);

    buf->busy      = true;
    currentBuffer_ = buf;
}

// ------------------------------------------------------------------
// 回调：registry / shm
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::registryGlobal(void* data, wl_registry* reg,
                                              uint32_t name, const char* iface,
                                              uint32_t version) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);

    if (std::strcmp(iface, wl_compositor_interface.name) == 0) {
        self->compositor_ = static_cast<wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface,
                             std::min(version, 4u)));
    } else if (std::strcmp(iface, wl_shm_interface.name) == 0) {
        self->shm_ = static_cast<wl_shm*>(
            wl_registry_bind(reg, name, &wl_shm_interface, 1));
        static const wl_shm_listener shmListener = { shmFormat };
        wl_shm_add_listener(self->shm_, &shmListener, self);
    } else if (std::strcmp(iface, wl_seat_interface.name) == 0) {
        self->seat_ = static_cast<wl_seat*>(
            wl_registry_bind(reg, name, &wl_seat_interface,
                             std::min(version, 5u)));
        static const wl_seat_listener seatListener = { seatCapabilities, seatName };
        wl_seat_add_listener(self->seat_, &seatListener, self);
    } else if (std::strcmp(iface, xdg_wm_base_interface.name) == 0) {
        self->wmBase_ = static_cast<xdg_wm_base*>(
            wl_registry_bind(reg, name, &xdg_wm_base_interface, 1));
        static const xdg_wm_base_listener wmListener = { wmBasePing };
        xdg_wm_base_add_listener(self->wmBase_, &wmListener, self);
    }
}

void LinuxWaylandNativeWidget::registryGlobalRemove(void*, wl_registry*, uint32_t) {}
void LinuxWaylandNativeWidget::shmFormat(void*, wl_shm*, uint32_t) {}

// ------------------------------------------------------------------
// 回调：seat / keyboard / pointer
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::seatCapabilities(void* data, wl_seat* seat, uint32_t caps) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->keyboard_) {
        self->keyboard_ = wl_seat_get_keyboard(seat);
        static const wl_keyboard_listener kbListener = {
            keyboardKeymap, keyboardEnter, keyboardLeave,
            keyboardKey, keyboardModifiers, keyboardRepeatInfo
        };
        wl_keyboard_add_listener(self->keyboard_, &kbListener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard_) {
        wl_keyboard_destroy(self->keyboard_);
        self->keyboard_ = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !self->pointer_) {
        self->pointer_ = wl_seat_get_pointer(seat);
        static const wl_pointer_listener ptrListener = {
            pointerEnter, pointerLeave, pointerMotion, pointerButton,
            pointerAxis, pointerFrame, pointerAxisSource,
            pointerAxisStop, pointerAxisDiscrete
        };
        wl_pointer_add_listener(self->pointer_, &ptrListener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && self->pointer_) {
        wl_pointer_destroy(self->pointer_);
        self->pointer_ = nullptr;
    }
}

void LinuxWaylandNativeWidget::seatName(void*, wl_seat*, const char*) {}

void LinuxWaylandNativeWidget::keyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t) {}
void LinuxWaylandNativeWidget::keyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
void LinuxWaylandNativeWidget::keyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*) {}

void LinuxWaylandNativeWidget::keyboardKey(void* data, wl_keyboard*, uint32_t,
                                           uint32_t, uint32_t key, uint32_t state) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    if (!self->eventCb_) return;

    Event evt{};
    evt.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                   ? EventType::KeyDown : EventType::KeyUp;
    evt.key.keycode = static_cast<int>(key) + 8; // 与 X11 keycode 习惯对齐
    evt.key.ch      = 0;
    evt.key.ctrl    = false;
    evt.key.shift   = false;
    evt.key.alt     = false;
    self->eventCb_(evt);
}

void LinuxWaylandNativeWidget::keyboardModifiers(void*, wl_keyboard*, uint32_t,
                                                 uint32_t, uint32_t, uint32_t, uint32_t) {}
void LinuxWaylandNativeWidget::keyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

void LinuxWaylandNativeWidget::pointerEnter(void*, wl_pointer*, uint32_t,
                                            wl_surface*, wl_fixed_t, wl_fixed_t) {}
void LinuxWaylandNativeWidget::pointerLeave(void*, wl_pointer*, uint32_t, wl_surface*) {}

void LinuxWaylandNativeWidget::pointerMotion(void* data, wl_pointer*, uint32_t,
                                             wl_fixed_t sx, wl_fixed_t sy) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    if (!self->eventCb_) return;
    Event evt{};
    evt.type    = EventType::MouseMove;
    evt.mouse.x = wl_fixed_to_int(sx);
    evt.mouse.y = wl_fixed_to_int(sy);
    self->eventCb_(evt);
}

void LinuxWaylandNativeWidget::pointerButton(void* data, wl_pointer*, uint32_t,
                                             uint32_t, uint32_t button, uint32_t state) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    if (!self->eventCb_) return;
    Event evt{};
    evt.type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
                   ? EventType::MouseDown : EventType::MouseUp;
    switch (button) {
        case BTN_LEFT:   evt.mouse.button = 1; break;
        case BTN_MIDDLE: evt.mouse.button = 2; break;
        case BTN_RIGHT:  evt.mouse.button = 3; break;
        default:         evt.mouse.button = static_cast<int>(button); break;
    }
    self->eventCb_(evt);
}

void LinuxWaylandNativeWidget::pointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}
void LinuxWaylandNativeWidget::pointerFrame(void*, wl_pointer*) {}
void LinuxWaylandNativeWidget::pointerAxisSource(void*, wl_pointer*, uint32_t) {}
void LinuxWaylandNativeWidget::pointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
void LinuxWaylandNativeWidget::pointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}

// ------------------------------------------------------------------
// 回调：xdg
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::wmBasePing(void*, xdg_wm_base* base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

void LinuxWaylandNativeWidget::xdgSurfaceConfigure(void* data, xdg_surface* surf, uint32_t serial) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    xdg_surface_ack_configure(surf, serial);

    if (!self->configured_) {
        self->configured_ = true;
        self->ensureBuffers(self->width_, self->height_);
        self->invalidateAll();
    }
}

void LinuxWaylandNativeWidget::toplevelConfigure(void* data, xdg_toplevel*,
                                                 int32_t w, int32_t h, wl_array*) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    if (w > 0 && h > 0) self->handleConfigure(w, h);
}

void LinuxWaylandNativeWidget::toplevelClose(void* data, xdg_toplevel*) {
    auto* self = static_cast<LinuxWaylandNativeWidget*>(data);
    self->running_ = false;
    if (self->eventCb_) {
        Event evt{};
        evt.type = EventType::Close;
        self->eventCb_(evt);
    }
}

void LinuxWaylandNativeWidget::toplevelConfigureBounds(void*, xdg_toplevel*, int32_t, int32_t) {}
void LinuxWaylandNativeWidget::toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

void LinuxWaylandNativeWidget::handleConfigure(int w, int h) {
    if (w == width_ && h == height_) return;
    width_  = w;
    height_ = h;
    ensureBuffers(w, h);

    if (eventCb_) {
        Event evt{};
        evt.type          = EventType::Resize;
        evt.resize.width  = w;
        evt.resize.height = h;
        eventCb_(evt);
    }
    invalidateAll();
}

// ------------------------------------------------------------------
// 回调
// ------------------------------------------------------------------
void LinuxWaylandNativeWidget::setPaintCallback(PaintCallback cb) { paintCb_ = std::move(cb); }
void LinuxWaylandNativeWidget::setEventCallback(EventCallback cb) { eventCb_ = std::move(cb); }

// ------------------------------------------------------------------
// 工厂
// ------------------------------------------------------------------
std::unique_ptr<Widget> createWaylandWidget() {
    auto w = std::make_unique<LinuxWaylandNativeWidget>();
    if (!w->create("UI", 800, 600)) return nullptr;
    return w;
}

} // namespace ui
#endif // __linux__