#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../../component/base/types.h"
#include "../../render/render.hpp"

namespace ui {

// ============================================================
// Widget：平台窗口
// ============================================================
class Widget {
public:
    virtual ~Widget() = default;

    // ---------- 生命周期 ----------
    virtual bool create(const std::string& title, int w, int h) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;

    // ---------- 主循环 ----------
    // timeoutMs < 0  : 无限阻塞，直到有事件（全堵塞）
    // timeoutMs == 0 : 非阻塞，立即返回
    // timeoutMs > 0  : 最多等这么久（毫秒）
    // 返回 false 表示应退出
    virtual bool pumpEvents(int timeoutMs = -1) = 0;

    // ---------- 属性 ----------
    virtual void setTitle(const std::string& title) = 0;
    virtual void setSize(int w, int h) = 0;
    virtual void getSize(int& w, int& h) const = 0;

    // ---------- 脏矩形 ----------
    virtual void invalidate(const Rect& r) = 0;
    virtual void invalidateAll() = 0;
    virtual void repaintNow() = 0;

    // ---------- 回调 ----------
    // 绘制：dirty 是本次需要重绘的区域；实现方必须至少保证 dirty 内被重绘
    using PaintCallback = std::function<void(Canvas& canvas, const Rect& dirty)>;
    virtual void setPaintCallback(PaintCallback cb) = 0;

    // 事件：统一签名 void(const Event&)
    using EventCallback = std::function<void(const Event&)>;
    virtual void setEventCallback(EventCallback cb) = 0;

    // ---------- 工厂 ----------
    static std::unique_ptr<Widget> create();
};

} // namespace ui