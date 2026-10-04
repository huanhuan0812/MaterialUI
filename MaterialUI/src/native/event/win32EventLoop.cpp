// src/native/event/win32_event_loop.cpp
#include "EventLoop.hpp"
#include "../NativeWidget/NativeWidget.h"

#if defined(_WIN32)
#include <windows.h>
#include <memory>

namespace ui {
namespace {

class Win32EventLoop : public EventLoop {
public:
    void attach(NativeWidget* widget) override {
        widget_ = widget;
    }

    void detach(NativeWidget* widget) override {
        if (widget_ == widget) {
            widget_ = nullptr;
        }
    }

    int run() override {
        while (!shouldQuit()) {
            if (!step(-1)) break;
        }
        return 0;
    }

    void quit() override {
        quit_ = true;
        PostQuitMessage(0);
    }

    bool step(int timeoutMs) override {
        if (shouldQuit()) return false;

        DWORD wait = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
        DWORD r = MsgWaitForMultipleObjects(0, nullptr, FALSE, wait, QS_ALLINPUT);

        if (r == WAIT_OBJECT_0) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    quit_ = true;
                    return false;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        return !shouldQuit();
    }

private:
    bool shouldQuit() const {
        if (quit_) return true;
        if (widget_ && widget_->shouldQuit()) return true;
        return false;
    }

    NativeWidget* widget_ = nullptr;
    bool quit_ = false;
};

} // namespace

std::unique_ptr<EventLoop> createWin32EventLoop() {
    return std::make_unique<Win32EventLoop>();
}

} // namespace ui
#endif // _WIN32