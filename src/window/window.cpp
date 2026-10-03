#include "window.hpp"

#include <algorithm>

namespace ui {

Window::Window() = default;

Window::Window(const std::string& title, int w, int h) {
    // 不在这里 create，等 factory 设置后再 create
    (void)title; (void)w; (void)h;
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

bool Window::create(const std::string& title, int w, int h) {
    if (widget_) return false;
    widget_ = NativeWidget::create();
    if (!widget_) return false;
    if (!widget_->create(title, w, h)) {
        widget_.reset();
        return false;
    }

    if (widgetFactory_) {
        root_ = widgetFactory_();
        if (root_) {
            root_->setGeometry({0, 0, w, h});
            root_->setRedrawCallback([this]() {
                if (widget_) widget_->invalidateAll();
            });
        }
    }

    bindCallbacks();
    return true;
}

void Window::show() {
    if (widget_) {
        widget_->show();
    }
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
        cb();
    }
}

void Window::setTitle(const std::string& title) {
    if (widget_) widget_->setTitle(title);
}

void Window::setSize(int w, int h) {
    if (widget_) widget_->setSize(w, h);
    if (root_)   root_->setGeometry({0, 0, w, h});
}

void Window::getSize(int& w, int& h) const {
    if (widget_) {
        widget_->getSize(w, h);
    } else {
        w = h = 0;
    }
}

void Window::invalidate(const Rect& r) {
    if (widget_) widget_->invalidate(r);
}

void Window::invalidateAll() {
    if (widget_) widget_->invalidateAll();
}

void Window::repaintNow() {
    if (widget_) widget_->repaintNow();
}

void Window::setPaintHandler(PaintHandler handler) {
    paintHandler_ = std::move(handler);
}

void Window::setEventHandler(EventHandler handler) {
    eventHandler_ = std::move(handler);
}

bool Window::shouldQuit() const {
    return widget_ ? widget_->shouldQuit() : true;
}

void Window::onPaint(Canvas& canvas, const Rect& dirty) {
    if (root_) {
        root_->render(canvas, 0, 0);
    }
    if (paintHandler_) paintHandler_(canvas, dirty);
}

void Window::onEvent(const Event& event) {
    if (event.type == EventType::Resize && root_) {
        root_->setGeometry({0, 0, event.width, event.height});
        if (widget_) widget_->invalidateAll();
    }

    if (root_ && root_->dispatchEvent(event)) {
        if (eventHandler_) eventHandler_(event);
        return;
    }
    if (eventHandler_) eventHandler_(event);
}

void Window::bindCallbacks() {
    if (!widget_) return;

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
                cb();
            }
        });
}

} // namespace ui