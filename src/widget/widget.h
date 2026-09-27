#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../component/base/types.h"
#include "../render/canvas.hpp"

namespace ui {

// ============================================================
// Canvas：绘图上下文（对外只暴露这个，平台句柄藏在实现里）
// ============================================================
// class Canvas {
// public:
//     virtual ~Canvas() = default;

//     // ---------- 已有 4 个纯虚（现有 Win32/Cocoa 已实现）----------
//     virtual void fillRect(const Rect& r, Color c) = 0;
//     virtual void drawRect(const Rect& r, Color c, int lineWidth = 1) = 0;
//     virtual void drawLine(int x0, int y0, int x1, int y1, Color c) = 0;
//     virtual void drawText(const std::string& text, int x, int y, Color c) = 0;

//     // ---------- 圆角 / 圆 ----------
//     virtual void fillRoundRect(const Rect&, int /*radius*/, Color) {}
//     virtual void drawRoundRect(const Rect&, int /*radius*/, Color, int /*lw*/ = 1) {}
//     virtual void fillCircle(Point, int /*r*/, Color) {}
//     virtual void drawCircle(Point, int /*r*/, Color, int /*lw*/ = 1) {}

//     // ---------- 阴影 / 卡片 ----------
//     // elevation 用 Material 规范（0,1,2,3,4,6,8,12,16,24）
//     virtual void drawShadow(const Rect&, int /*radius*/,
//                             int /*elevation*/, Color = 0x3C000000) {}
//     virtual void drawCard(const Rect&, int /*radius*/, Color,
//                           int /*elevation*/ = 1) {}

//     // ---------- 渐变 ----------
//     virtual void fillLinearGradient(const Rect&, Color, Color,
//                                     bool /*vertical*/ = true) {}

//     // ---------- 文本扩展 ----------
//     enum class TextAlign { Left, Center, Right };
//     virtual void drawTextAligned(const std::string&, const Rect&,
//                                  Color, TextAlign = TextAlign::Left,
//                                  bool /*vcenter*/ = true) {}
//     virtual void setFontSize(int /*px*/) {}
//     virtual void setFontBold(bool /*bold*/) {}

//     // ---------- 状态栈 ----------
//     virtual void save() {}
//     virtual void restore() {}
//     virtual void translate(int /*dx*/, int /*dy*/) {}
//     virtual void clipRect(const Rect&) {}
// };

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