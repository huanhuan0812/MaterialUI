// src/native/event/cocoa_event_loop.mm
#include "EventLoop.hpp"
#include "../NativeWidget/NativeWidget.h"

#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#include <memory>

namespace ui {
namespace {

class CocoaEventLoop : public EventLoop {
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
        [NSApp stop:nil];
        // 唤醒 runloop，避免 stop 后仍阻塞
        NSEvent* ev = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                         location:NSZeroPoint
                                    modifierFlags:0
                                        timestamp:0
                                     windowNumber:0
                                          context:nil
                                          subtype:0
                                            data1:0
                                            data2:0];
        [NSApp postEvent:ev atStart:YES];
    }

    bool step(int timeoutMs) override {
        if (shouldQuit()) return false;

        @autoreleasepool {
            NSDate* until;
            if (timeoutMs < 0) {
                until = [NSDate distantFuture];
            } else if (timeoutMs == 0) {
                until = [NSDate distantPast];
            } else {
                until = [NSDate dateWithTimeIntervalSinceNow:timeoutMs / 1000.0];
            }

            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:until
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
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

std::unique_ptr<EventLoop> createCocoaEventLoop() {
    return std::make_unique<CocoaEventLoop>();
}

} // namespace ui
#endif // __APPLE__