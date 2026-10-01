// src/ui/application.cpp
#include "application.hpp"

#include <algorithm>

namespace ui {

Application::Application()
    : loop_(EventLoop::create()) {
}

Application::~Application() {
    // 先关闭所有窗口（触发 onWindowClosed -> pendingDestroy_）
    windows_.clear();
    pendingDestroy_.clear();
}

// ---------- 窗口管理 ----------

Window* Application::createWindow(const std::string& title, int w, int h) {
    if (!loop_) return nullptr;

    auto win = std::make_unique<Window>(title, w, h);
    if (!win->isValid()) return nullptr;

    Window* raw = win.get();

    // 关键：把 Window 的关闭/析构回调到 Application
    // 需要在 Window 里加一个 setCloseCallback（见下方说明）
    raw->setCloseCallback([this, raw]() {
        this->onWindowClosed(raw);
    });

    windows_.push_back(std::move(win));
    return raw;
}

void Application::destroyWindow(Window* w) {
    if (!w) return;
    // 延迟到下一帧 flush，避免在事件处理中途 delete
    if (std::find(pendingDestroy_.begin(), pendingDestroy_.end(), w)
        == pendingDestroy_.end()) {
        pendingDestroy_.push_back(w);
    }
}

void Application::onWindowClosed(Window* w) {
    destroyWindow(w);
}

// ---------- 生命周期 ----------

int Application::run() {
    if (!loop_) return -1;

    running_ = true;
    shouldQuit_ = false;

    // 若已有窗口，attach 第一个作为退出锚点（可选）
    // 但更推荐 Application 自己控制循环，不依赖 EventLoop::run
    while (!shouldQuit_) {
        // 1. 处理 pending 销毁
        flushDestroy();

        // 2. 没有窗口且用户没主动 quit -> 自动退出
        if (windows_.empty() && !shouldQuit_) {
            break;
        }

        // 3. 单步事件循环（阻塞直到有事件或超时）
        //    即使没有窗口，也可能需要处理 native 事件，所以传一个合理超时
        bool alive = loop_->step(windows_.empty() ? 0 : -1);
        if (!alive) break;

        // 4. 每帧刷新（重绘、动画等）
        flushWindows();
    }

    running_ = false;
    return 0;
}

void Application::quit() {
    shouldQuit_ = true;
    if (loop_) loop_->quit();
}

// ---------- 内部 ----------

void Application::flushDestroy() {
    for (Window* w : pendingDestroy_) {
        auto it = std::find_if(windows_.begin(), windows_.end(),
            [w](const std::unique_ptr<Window>& p) { return p.get() == w; });
        if (it != windows_.end()) {
            // 从 EventLoop 解绑（如果已 attach）
            if (loop_) loop_->detach(it->get()->nativeWidget());
            windows_.erase(it);
        }
    }
    pendingDestroy_.clear();
}

void Application::flushWindows() {
    for (auto& w : windows_) {
        if (w->isValid()) {
            // 例如：驱动每帧重绘
            // w->repaintIfDirty();
        }
    }
}

} // namespace ui