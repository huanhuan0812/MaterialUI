#include "application.hpp"

#include <algorithm>

namespace ui {

Application::Application()
    : loop_(EventLoop::create()) {}

Application::~Application() {
    windows_.clear();
    pendingDestroy_.clear();
}

Window* Application::createWindow(const std::string& title, int w, int h) {
    if (!loop_) return nullptr;

    auto win = std::make_unique<Window>();
    Window* raw = win.get();
    raw->setCloseCallback([this, raw]() { this->onWindowClosed(raw); });

    raw->setWidgetFactory([this]() -> std::shared_ptr<Widget> {
        auto root = std::make_shared<Widget>();
        Handle h = widgets_.add(root);
        root->attach(h, &widgets_);
        return root;
    });

    if (!raw->create(title, w, h)) return nullptr;

    // Attach native widget to event loop
    if (raw->nativeWidget()) {
        loop_->attach(raw->nativeWidget());
    }

    windows_.push_back(std::move(win));
    return raw;
}

void Application::destroyWindow(Window* w) {
    if (!w) return;
    if (std::find(pendingDestroy_.begin(), pendingDestroy_.end(), w)
        == pendingDestroy_.end()) {
        pendingDestroy_.push_back(w);
    }
}

void Application::onWindowClosed(Window* w) { destroyWindow(w); }

int Application::run() {
    if (!loop_) return -1;
    running_ = true;
    shouldQuit_ = false;

    while (!shouldQuit_) {
        flushDestroy();
        if (windows_.empty() && !shouldQuit_) break;
        bool alive = loop_->step(windows_.empty() ? 0 : -1);
        if (!alive) break;
    }
    running_ = false;
    return 0;
}

void Application::quit() {
    shouldQuit_ = true;
    if (loop_) loop_->quit();
}

void Application::flushDestroy() {
    for (Window* w : pendingDestroy_) {
        auto it = std::find_if(windows_.begin(), windows_.end(),
            [w](const std::unique_ptr<Window>& p) { return p.get() == w; });
        if (it != windows_.end()) {
            if (loop_) loop_->detach(it->get()->nativeWidget());
            windows_.erase(it);
        }
    }
    pendingDestroy_.clear();
}

} // namespace ui