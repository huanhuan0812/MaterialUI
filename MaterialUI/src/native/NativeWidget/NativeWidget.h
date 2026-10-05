// src/native/NativeWidget/NativeWidget.h
#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../../core/types.h"
#include "../../canvas/canvas.hpp"

namespace ui {

class EventLoop;

// ============================================================
// NativeWidget：平台窗口
// ============================================================
class NativeWidget {
public:
    virtual ~NativeWidget() = default;

    // ---------- 生命周期 ----------
    virtual bool create(const std::string& title, int w, int h) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;

    // ---------- 属性 ----------
    virtual void setTitle(const std::string& title) = 0;
    virtual void setSize(int w, int h) = 0;
    virtual void getSize(int& w, int& h) const = 0;

    // ---------- 脏矩形 ----------
    virtual void invalidate(const Rect& r) = 0;
    virtual void invalidateAll() = 0;
    virtual void repaintNow() = 0;

    // ---------- 回调 ----------
    using PaintCallback = std::function<void(Canvas& canvas, const Rect& dirty)>;
    virtual void setPaintCallback(PaintCallback cb) = 0;

    using EventCallback = std::function<void(const Event&)>;
    virtual void setEventCallback(EventCallback cb) = 0;

    using CloseCallback = std::function<void()>;
    virtual void setCloseCallback(CloseCallback cb) = 0;

    // ---------- 与 EventLoop 关联 ----------
    // 窗口关闭后，EventLoop 通过它判断是否应退出
    virtual bool shouldQuit() const = 0;

    // ---------- 工厂 ----------
    static std::unique_ptr<NativeWidget> create();
};

} // namespace ui