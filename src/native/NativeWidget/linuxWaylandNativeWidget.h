#pragma once

#include "NativeWidget.h"

#if defined(__linux__)
#pragma once

#include "NativeWidget.h"

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <cairo/cairo.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ui {

class LinuxWaylandNativeWidget final : public NativeWidget {
public:
    LinuxWaylandNativeWidget();
    ~LinuxWaylandNativeWidget() override;

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
    struct ShmBuffer {
        wl_buffer* buffer = nullptr;
        void*      data   = nullptr;
        size_t     size   = 0;
        int        w      = 0;
        int        h      = 0;
        bool       busy   = false;
    };

    bool       createShmBuffer(ShmBuffer& buf, int w, int h);
    void       destroyBuffer(ShmBuffer& buf);
    ShmBuffer* nextFreeBuffer();
    void       ensureBuffers(int w, int h);
    void       doPaint();
    void       handleConfigure(int w, int h);

    // 全局对象
    wl_display*    display_    = nullptr;
    wl_registry*   registry_   = nullptr;
    wl_compositor* compositor_ = nullptr;
    wl_shm*        shm_        = nullptr;
    wl_seat*       seat_       = nullptr;
    wl_keyboard*   keyboard_   = nullptr;
    wl_pointer*    pointer_    = nullptr;
    xdg_wm_base*   wmBase_     = nullptr;

    wl_surface*    surface_    = nullptr;
    xdg_surface*   xdgSurface_ = nullptr;
    xdg_toplevel*  xdgToplevel_= nullptr;

    std::vector<ShmBuffer> buffers_;
    ShmBuffer* currentBuffer_ = nullptr;

    int         width_      = 0;
    int         height_     = 0;
    std::string title_;
    bool        running_    = false;
    bool        visible_    = false;
    bool        configured_ = false;

    std::atomic<bool> needsRepaint_{false};
    bool        dirty_ = false;
    Rect        dirtyRect_{};
    std::mutex  paintMutex_;

    PaintCallback paintCb_;
    EventCallback eventCb_;

    // ---- 静态回调桥接 ----
    static void registryGlobal(void* data, wl_registry* reg, uint32_t name,
                               const char* iface, uint32_t version);
    static void registryGlobalRemove(void* data, wl_registry* reg, uint32_t name);
    static void shmFormat(void* data, wl_shm* shm, uint32_t format);

    static void seatCapabilities(void* data, wl_seat* seat, uint32_t caps);
    static void seatName(void* data, wl_seat* seat, const char* name);

    static void keyboardKeymap(void* data, wl_keyboard* kb, uint32_t format,
                               int32_t fd, uint32_t size);
    static void keyboardEnter(void* data, wl_keyboard* kb, uint32_t serial,
                              wl_surface* surf, wl_array* keys);
    static void keyboardLeave(void* data, wl_keyboard* kb, uint32_t serial,
                              wl_surface* surf);
    static void keyboardKey(void* data, wl_keyboard* kb, uint32_t serial,
                            uint32_t time, uint32_t key, uint32_t state);
    static void keyboardModifiers(void* data, wl_keyboard* kb, uint32_t serial,
                                  uint32_t dep, uint32_t lat,
                                  uint32_t lock, uint32_t group);
    static void keyboardRepeatInfo(void* data, wl_keyboard* kb,
                                   int32_t rate, int32_t delay);

    static void pointerEnter(void* data, wl_pointer* p, uint32_t serial,
                             wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy);
    static void pointerLeave(void* data, wl_pointer* p, uint32_t serial,
                             wl_surface* surf);
    static void pointerMotion(void* data, wl_pointer* p, uint32_t time,
                              wl_fixed_t sx, wl_fixed_t sy);
    static void pointerButton(void* data, wl_pointer* p, uint32_t serial,
                              uint32_t time, uint32_t button, uint32_t state);
    static void pointerAxis(void* data, wl_pointer* p, uint32_t time,
                            uint32_t axis, wl_fixed_t value);
    static void pointerFrame(void* data, wl_pointer* p);
    static void pointerAxisSource(void* data, wl_pointer* p, uint32_t src);
    static void pointerAxisStop(void* data, wl_pointer* p, uint32_t time,
                                uint32_t axis);
    static void pointerAxisDiscrete(void* data, wl_pointer* p,
                                    uint32_t axis, int32_t discrete);

    static void wmBasePing(void* data, xdg_wm_base* base, uint32_t serial);
    static void xdgSurfaceConfigure(void* data, xdg_surface* surf, uint32_t serial);
    static void toplevelConfigure(void* data, xdg_toplevel* tl,
                                  int32_t w, int32_t h, wl_array* states);
    static void toplevelClose(void* data, xdg_toplevel* tl);
    static void toplevelConfigureBounds(void* data, xdg_toplevel* tl,
                                        int32_t w, int32_t h);
    static void toplevelWmCapabilities(void* data, xdg_toplevel* tl,
                                       wl_array* caps);
};

// 由 NativeWidget.cpp 调用
std::unique_ptr<Widget> createWaylandWidget();

} // namespace ui
#endif // __linux__