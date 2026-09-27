// src/ui/application.h
#pragma once
#include "../native/native_widget/NativeWidget.h"
#include <memory>

namespace ui {

class Application {
public:
    Application() {
        window_ = Widget::create();   // 复用你现有的平台工厂
    }

    // 用户不需要知道 pumpEvents 的细节
    int exec() {
        if (!window_) return -1;
        while (window_->pumpEvents()) {   // 阻塞等待事件
            tick();
        }
        return 0;
    }

    void quit() { window_->close(); }

    Widget* native() { return window_.get(); }  // 供 Window 内部使用

private:
    void tick() {
        // 这里可以调用 pending 的重绘、动画
    }

    std::unique_ptr<Widget> window_;
};

} // namespace ui