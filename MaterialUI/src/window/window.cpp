#include "window.hpp"

namespace ui {

Window::Window() = default;

Window::Window(const std::string& title, int w, int h) {
    create(title, w, h);
}

Window::~Window() {
    if (widget_) {
        widget_->close();
        widget_.reset();
    }
    if (closeCallback_) {
        auto cb = std::move(closeCallback_);
        closeCallback_ = nullptr;
        cb();
    }
}

Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

// ---------- 生命周期 ----------

bool Window::create(const std::string& title, int w, int h) {
    if (widget_) return false;               // 已创建
    widget_ = NativeWidget::create();              // 平台工厂
    if (!widget_) return false;              // 平台不支持
    if (!widget_->create(title, w, h)) {     // 原生创建失败
        widget_.reset();
        return false;
    }
    bindCallbacks();
    return true;
}

void Window::show() {
    if (widget_) widget_->show();
}

void Window::hide() {
    if (widget_) widget_->hide();
}

void Window::close() {
    if (widget_) {
        widget_->close();
        widget_.reset();
    }
    if (closeCallback_) {
        auto cb = std::move(closeCallback_);
        closeCallback_ = nullptr;
        cb();          // 通知 Application 移除
    }
}

// ---------- 属性 ----------

void Window::setTitle(const std::string& title) {
    if (widget_) widget_->setTitle(title);
}

void Window::setSize(int w, int h) {
    if (widget_) widget_->setSize(w, h);
}

void Window::getSize(int& w, int& h) const {
    if (widget_) {
        widget_->getSize(w, h);
    } else {
        w = h = 0;
    }
}

// ---------- 重绘 ----------

void Window::invalidate(const Rect& r) {
    if (widget_) widget_->invalidate(r);
}

void Window::invalidateAll() {
    if (widget_) widget_->invalidateAll();
}

void Window::repaintNow() {
    if (widget_) widget_->repaintNow();
}

// ---------- 回调 ----------

void Window::setPaintHandler(PaintHandler handler) {
    paintHandler_ = std::move(handler);
}

void Window::setEventHandler(EventHandler handler) {
    eventHandler_ = std::move(handler);
}

// ---------- EventLoop 查询 ----------

bool Window::shouldQuit() const {
    return widget_ ? widget_->shouldQuit() : true;
}

// ---------- 默认虚钩子 ----------

void Window::onPaint(Canvas& canvas, const Rect& dirty) {
    if (paintHandler_) {
        paintHandler_(canvas, dirty);
    }
}

void Window::onEvent(const Event& event) {
    if (eventHandler_) {
        eventHandler_(event);
    }
}

// ---------- 绑定 Widget 回调到本类 ----------

void Window::bindCallbacks() {
    if (!widget_) return;

    // 捕获 this 安全：Window 析构时会先 close，不再触发回调
    widget_->setPaintCallback(
        [this](Canvas& canvas, const Rect& dirty) {
            this->onPaint(canvas, dirty);
        });

    widget_->setEventCallback(
        [this](const Event& event) {
            this->onEvent(event);
        });
    widget_->setCloseCallback(
        [this]() {
            if (closeCallback_) {
                auto cb = std::move(closeCallback_);
                closeCallback_ = nullptr;
                cb();          // 通知 Application 移除
            }
        });
}

} // namespace ui