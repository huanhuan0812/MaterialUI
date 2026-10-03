#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../native/NativeWidget/NativeWidget.h"
#include "../component/widget.hpp"

namespace ui {

class EventLoop;

class Window {
public:
    using PaintHandler = std::function<void(Canvas& canvas, const Rect& dirty)>;
    using EventHandler = std::function<void(const Event&)>;
    using WidgetFactory = std::function<std::shared_ptr<Widget>()>;

    Window();
    Window(const std::string& title, int w, int h);
    virtual ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool create(const std::string& title, int w, int h);
    bool isValid() const { return widget_ != nullptr; }

    void show();
    void hide();
    void close();

    void setTitle(const std::string& title);
    void setSize(int w, int h);
    void getSize(int& w, int& h) const;

    void invalidate(const Rect& r);
    void invalidateAll();
    void repaintNow();

    void setPaintHandler(PaintHandler handler);
    void setEventHandler(EventHandler handler);

    bool shouldQuit() const;

    NativeWidget* nativeWidget() const { return widget_.get(); }

    void setCloseCallback(std::function<void()> cb) { closeCallback_ = std::move(cb); }

    void setWidgetFactory(WidgetFactory f) { widgetFactory_ = std::move(f); }
    Widget* root() const { return root_.get(); }

    void addChild(std::shared_ptr<Widget> c) {
        if (root_) root_->addChild(std::move(c));
    }

protected:
    virtual void onPaint(Canvas& canvas, const Rect& dirty);
    virtual void onEvent(const Event& event);

private:
    void bindCallbacks();

    std::unique_ptr<NativeWidget> widget_;
    PaintHandler paintHandler_;
    EventHandler eventHandler_;
    std::function<void()> closeCallback_;

    WidgetFactory widgetFactory_;
    std::shared_ptr<Widget> root_;
};

} // namespace ui