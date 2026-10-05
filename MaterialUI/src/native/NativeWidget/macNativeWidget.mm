// src/native/NativeWidget/macNativeWidget.mm
#include "NativeWidget.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <string>
#include <memory>
#include <utility>

#include "../../canvas/cocoaCanvas.mm"

// ---- 前向声明（非匿名命名空间）----
namespace ui {
class CocoaWidget;
}

// ---- ObjC 委托 / 视图 ----
@interface UiWidgetDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate>
@property (nonatomic, assign) ui::CocoaWidget* owner;
@end

@interface UiWidgetView : NSView
@property (nonatomic, assign) ui::CocoaWidget* owner;
@property (nonatomic, assign) BOOL trackingMouse;
@end

namespace ui {

// ============================================================
// CocoaWidget
// ============================================================
class CocoaWidget : public NativeWidget {
public:
    CocoaWidget() {
        delegate_ = [[UiWidgetDelegate alloc] init];
        delegate_.owner = this;
    }
    bool shouldQuit() const override { return shouldQuit_; }
    ~CocoaWidget() override {
        if (window_) {
            [window_ setDelegate:nil];
            [window_ close];
            window_ = nil;
        }
        view_ = nil;
        delegate_ = nil;
    }

    // ---------- 对外：事件入口（给 ObjC 视图调用）----------
    void emit(Event e) {
        if (eventCb_) eventCb_(e);
    }

    // ---------- Widget 接口 ----------
    bool create(const std::string& title, int w, int h) override {
        @autoreleasepool {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            if (![NSApp delegate]) {
                [NSApp setDelegate:delegate_];
            }

            NSRect frame = NSMakeRect(0, 0, w, h);
            NSWindowStyleMask style =
                NSWindowStyleMaskTitled |
                NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable |
                NSWindowStyleMaskResizable;

            window_ = [[NSWindow alloc] initWithContentRect:frame
                                                  styleMask:style
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
            if (!window_) return false;

            window_.delegate = delegate_;
            window_.title = [NSString stringWithUTF8String:title.c_str()];
            window_.releasedWhenClosed = NO;

            view_ = [[UiWidgetView alloc] initWithFrame:frame];
            view_.owner = this;
            view_.trackingMouse = NO;
            window_.contentView = view_;

            [window_ center];
            return true;
        }
    }

    void show() override {
        [window_ makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }

    void hide() override { [window_ orderOut:nil]; }

    void close() override {
        shouldQuit_ = true;
        if (window_) [window_ close];
    }

    void setTitle(const std::string& t) override {
        window_.title = [NSString stringWithUTF8String:t.c_str()];
    }

    void setSize(int w, int h) override {
        [window_ setContentSize:NSMakeSize(w, h)];
    }

    void getSize(int& w, int& h) const override {
        NSSize s = view_.bounds.size;
        w = static_cast<int>(s.width);
        h = static_cast<int>(s.height);
    }

    void invalidate(const Rect& r) override {
        NSRect nr = NSMakeRect(r.x, r.y, r.w, r.h);
        [view_ setNeedsDisplayInRect:nr];
    }
    void invalidateAll() override {
        [view_ setNeedsDisplay:YES];
    }
    void repaintNow() override {
        [view_ displayIfNeeded];
    }

    void setPaintCallback(PaintCallback cb) override { paintCb_ = std::move(cb); }
    void setEventCallback(EventCallback cb) override { eventCb_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) override { closeCb_ = std::move(cb); }

    // ---------- 供 ObjC 调用 ----------
    void onPaint(CGContextRef ctx, const Rect& dirty) {
        if (paintCb_) {
            CocoaCanvas canvas(ctx);
            paintCb_(canvas, dirty);
        }
    }

    void onWindowClosed() { shouldQuit_ = true; if (closeCb_) closeCb_();}

private:
    NSWindow*         window_   = nil;
    UiWidgetView*     view_     = nil;
    UiWidgetDelegate* delegate_ = nil;
    bool shouldQuit_ = false;

    PaintCallback paintCb_;
    EventCallback eventCb_;
    CloseCallback closeCb_;
};

std::unique_ptr<NativeWidget> createCocoaWidget() {
    return std::make_unique<CocoaWidget>();
}

} // namespace ui

// ============================================================
// ObjC 实现
// ============================================================
@implementation UiWidgetDelegate

- (void)windowWillClose:(NSNotification*)notification {
    if (self.owner) self.owner->onWindowClosed();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app {
    return YES;
}

@end

@implementation UiWidgetView

- (BOOL)isFlipped { return YES; }          // 左上角原点
- (BOOL)acceptsFirstResponder { return YES; }

// ---------- 绘制 ----------
- (void)drawRect:(NSRect)dirtyRect {
    if (!self.owner) return;

    CGContextRef ctx = [NSGraphicsContext currentContext].CGContext;
    ui::Rect r{
        static_cast<int>(dirtyRect.origin.x),
        static_cast<int>(dirtyRect.origin.y),
        static_cast<int>(dirtyRect.size.width),
        static_cast<int>(dirtyRect.size.height)
    };
    self.owner->onPaint(ctx, r);
}

// ---------- 鼠标 ----------
- (void)mouseDown:(NSEvent*)e {
    if (!self.owner) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    ui::Event ev;
    ev.type = ui::EventType::MouseDown;
    ev.x = static_cast<int>(p.x);
    ev.y = static_cast<int>(p.y);
    self.owner->emit(ev);
}

- (void)mouseUp:(NSEvent*)e {
    if (!self.owner) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    ui::Event ev;
    ev.type = ui::EventType::MouseUp;
    ev.x = static_cast<int>(p.x);
    ev.y = static_cast<int>(p.y);
    self.owner->emit(ev);
}

- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }

- (void)mouseMoved:(NSEvent*)e {
    if (!self.owner) return;

    if (!self.trackingMouse) {
        NSTrackingAreaOptions opts =
            NSTrackingMouseEnteredAndExited |
            NSTrackingMouseMoved |
            NSTrackingActiveInKeyWindow |
            NSTrackingInVisibleRect;
        NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                            options:opts
                                                              owner:self
                                                           userInfo:nil];
        [self addTrackingArea:area];
        self.trackingMouse = YES;
    }

    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    ui::Event ev;
    ev.type = ui::EventType::MouseMove;
    ev.x = static_cast<int>(p.x);
    ev.y = static_cast<int>(p.y);
    self.owner->emit(ev);
}

- (void)mouseEntered:(NSEvent*)e {
    if (!self.owner) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    ui::Event ev;
    ev.type = ui::EventType::MouseEnter;
    ev.x = static_cast<int>(p.x);
    ev.y = static_cast<int>(p.y);
    self.owner->emit(ev);
}

- (void)mouseExited:(NSEvent*)e {
    if (!self.owner) return;
    ui::Event ev;
    ev.type = ui::EventType::MouseExit;
    self.owner->emit(ev);
}

// ---------- 滚轮 ----------
- (void)scrollWheel:(NSEvent*)e {
    if (!self.owner) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    ui::Event ev;
    ev.type = ui::EventType::Wheel;
    ev.x = static_cast<int>(p.x);
    ev.y = static_cast<int>(p.y);
    ev.wheelDelta = static_cast<int>(e.scrollingDeltaY);
    self.owner->emit(ev);
}

// ---------- 键盘 ----------
- (void)keyDown:(NSEvent*)e {
    if (!self.owner) return;
    ui::Event ev;
    ev.type = ui::EventType::KeyDown;
    ev.key = static_cast<int>(e.keyCode);
    ev.shift = (e.modifierFlags & NSEventModifierFlagShift) != 0;
    ev.ctrl  = (e.modifierFlags & NSEventModifierFlagControl) != 0;
    ev.alt   = (e.modifierFlags & NSEventModifierFlagOption) != 0;
    ev.meta  = (e.modifierFlags & NSEventModifierFlagCommand) != 0;
    self.owner->emit(ev);

    // 文本输入
    NSString* s = e.characters;
    if (s.length > 0) {
        ui::Event ce;
        ce.type = ui::EventType::Char;
        ce.codepoint = static_cast<uint32_t>([s characterAtIndex:0]);
        self.owner->emit(ce);
    }
}

- (void)keyUp:(NSEvent*)e {
    if (!self.owner) return;
    ui::Event ev;
    ev.type = ui::EventType::KeyUp;
    ev.key = static_cast<int>(e.keyCode);
    self.owner->emit(ev);
}

// ---------- 尺寸变化 ----------
- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (!self.owner) return;
    ui::Event ev;
    ev.type = ui::EventType::Resize;
    ev.width  = static_cast<int>(newSize.width);
    ev.height = static_cast<int>(newSize.height);
    self.owner->emit(ev);
}

@end