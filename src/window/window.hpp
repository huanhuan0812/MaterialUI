#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../native/NativeWidget/NativeWidget.h"   // 引入 Widget 定义

namespace ui {

// 前向声明
class EventLoop;

// ============================================================
// Window：对 Widget 的面向用户封装
// ============================================================
class Window {
public:
    // 绘制回调
    using PaintHandler = std::function<void(Canvas& canvas, const Rect& dirty)>;
    // 事件回调
    using EventHandler = std::function<void(const Event&)>;

    // 构造：不创建原生窗口（延迟创建，便于派生类）
    Window();
    // 构造并立即创建
    Window(const std::string& title, int w, int h);
    virtual ~Window();

    // 禁止拷贝，允许移动
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    // ---------- 生命周期 ----------
    // 显式创建；若已创建则返回 false
    bool create(const std::string& title, int w, int h);
    // 是否已创建
    bool isValid() const { return widget_ != nullptr; }

    void show();
    void hide();
    void close();

    // ---------- 属性 ----------
    void setTitle(const std::string& title);
    void setSize(int w, int h);
    void getSize(int& w, int& h) const;

    // ---------- 重绘 ----------
    void invalidate(const Rect& r);
    void invalidateAll();
    void repaintNow();

    // ---------- 回调设置 ----------
    void setPaintHandler(PaintHandler handler);
    void setEventHandler(EventHandler handler);

    // ---------- 供 EventLoop 使用 ----------
    bool shouldQuit() const;

    // ---------- 访问底层 Widget（高级用法） ----------
    NativeWidget* nativeWidget() const { return widget_.get(); }

    // ---------- 关闭回调（供 Application 使用） ----------
    void setCloseCallback(std::function<void()> cb) { closeCallback_ = std::move(cb); }

protected:
    // 派生类可覆盖的绘制钩子（比 std::function 更高效）
    virtual void onPaint(Canvas& canvas, const Rect& dirty);
    // 派生类可覆盖的事件钩子
    virtual void onEvent(const Event& event);

private:
    void bindCallbacks();   // 把 Widget 回调绑定到本类虚函数

    std::unique_ptr<NativeWidget> widget_;
    PaintHandler paintHandler_;
    EventHandler eventHandler_;
    std::function<void()> closeCallback_;
};

} // namespace ui