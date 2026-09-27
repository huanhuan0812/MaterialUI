// src/widget/widget_cocoa.mm
#include "widget.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <string>
#include <memory>
#include <utility>

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
// CocoaCanvas
// ============================================================
class CocoaCanvas : public Canvas {
public:
    explicit CocoaCanvas(CGContextRef ctx) : ctx_(ctx) {}

    // ---------- 基础 4 个 ----------
    void fillRect(const Rect& r, Color c) override {
        setFill(c);
        CGContextFillRect(ctx_, CGRectMake(r.x, r.y, r.w, r.h));
    }

    void drawRect(const Rect& r, Color c, int lineWidth) override {
        setStroke(c);
        CGContextSetLineWidth(ctx_, lineWidth);
        CGContextStrokeRect(ctx_, CGRectMake(r.x + 0.5, r.y + 0.5,
                                             r.w - 1, r.h - 1));
    }

    void drawLine(int x0, int y0, int x1, int y1, Color c) override {
        setStroke(c);
        CGContextSetLineWidth(ctx_, 1);
        CGContextBeginPath(ctx_);
        CGContextMoveToPoint(ctx_, x0 + 0.5, y0 + 0.5);
        CGContextAddLineToPoint(ctx_, x1 + 0.5, y1 + 0.5);
        CGContextStrokePath(ctx_);
    }

    void drawText(const std::string& text, int x, int y, Color c) override {
        @autoreleasepool {
            NSGraphicsContext* gc = [NSGraphicsContext currentContext];
            [gc saveGraphicsState];

            NSString* s = [NSString stringWithUTF8String:text.c_str()];
            NSColor* color = [NSColor colorWithCalibratedRed:redOf(c)   / 255.0
                                                       green:greenOf(c) / 255.0
                                                        blue:blueOf(c)  / 255.0
                                                       alpha:alphaOf(c) / 255.0];
            NSFont* font = bold_
                ? [NSFont boldSystemFontOfSize:fontSize_]
                : [NSFont systemFontOfSize:fontSize_];
            NSDictionary* attrs = @{
                NSForegroundColorAttributeName : color,
                NSFontAttributeName : font
            };
            [s drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];

            [gc restoreGraphicsState];
        }
    }

    // ---------- 圆角 / 圆 ----------
    void fillRoundRect(const Rect& r, int radius, Color c) override {
        setFill(c);
        CGPathRef path = CGPathCreateWithRoundedRect(
            CGRectMake(r.x, r.y, r.w, r.h), radius, radius, nullptr);
        CGContextAddPath(ctx_, path);
        CGContextFillPath(ctx_);
        CGPathRelease(path);
    }

    void drawRoundRect(const Rect& r, int radius, Color c, int lw) override {
        setStroke(c);
        CGContextSetLineWidth(ctx_, lw);
        CGPathRef path = CGPathCreateWithRoundedRect(
            CGRectMake(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1),
            radius, radius, nullptr);
        CGContextAddPath(ctx_, path);
        CGContextStrokePath(ctx_);
        CGPathRelease(path);
    }

    void fillCircle(Point c, int radius, Color col) override {
        setFill(col);
        CGContextFillEllipseInRect(ctx_,
            CGRectMake(c.x - radius, c.y - radius, radius * 2, radius * 2));
    }

    void drawCircle(Point c, int radius, Color col, int lw) override {
        setStroke(col);
        CGContextSetLineWidth(ctx_, lw);
        CGContextStrokeEllipseInRect(ctx_,
            CGRectMake(c.x - radius + 0.5, c.y - radius + 0.5,
                       radius * 2 - 1, radius * 2 - 1));
    }

    // ---------- 阴影 / 卡片 ----------
    void drawShadow(const Rect& r, int radius, int elevation,
                    Color shadow = 0x3C000000) override {
        if (elevation <= 0) return;
        CGContextSaveGState(ctx_);

        CGContextSetShadowWithColor(
            ctx_,
            CGSizeMake(0, elevation * 0.5),
            elevation * 2.0,
            [NSColor colorWithCalibratedRed:redOf(shadow)   / 255.0
                                      green:greenOf(shadow) / 255.0
                                       blue:blueOf(shadow)  / 255.0
                                      alpha:alphaOf(shadow) / 255.0].CGColor);

        setFill(shadow);
        CGPathRef path = CGPathCreateWithRoundedRect(
            CGRectMake(r.x, r.y, r.w, r.h), radius, radius, nullptr);
        CGContextAddPath(ctx_, path);
        CGContextFillPath(ctx_);
        CGPathRelease(path);

        CGContextRestoreGState(ctx_);
    }

    void drawCard(const Rect& r, int radius, Color fill,
                  int elevation = 1) override {
        if (elevation > 0) drawShadow(r, radius, elevation);
        fillRoundRect(r, radius, fill);
    }

    // ---------- 渐变 ----------
    void fillLinearGradient(const Rect& r, Color c0, Color c1,
                            bool vertical = true) override {
        CGContextSaveGState(ctx_);

        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGFloat comps[8] = {
            redOf(c0)   / 255.0, greenOf(c0) / 255.0,
            blueOf(c0)  / 255.0, alphaOf(c0) / 255.0,
            redOf(c1)   / 255.0, greenOf(c1) / 255.0,
            blueOf(c1)  / 255.0, alphaOf(c1) / 255.0,
        };
        CGFloat locs[2] = { 0.0, 1.0 };
        CGGradientRef grad = CGGradientCreateWithColorComponents(space, comps, locs, 2);

        CGPoint start = CGPointMake(r.x, r.y);
        CGPoint end   = vertical ? CGPointMake(r.x, r.y + r.h)
                                 : CGPointMake(r.x + r.w, r.y);

        CGContextClipToRect(ctx_, CGRectMake(r.x, r.y, r.w, r.h));
        CGContextDrawLinearGradient(ctx_, grad, start, end, 0);

        CGGradientRelease(grad);
        CGColorSpaceRelease(space);
        CGContextRestoreGState(ctx_);
    }

    // ---------- 文本扩展 ----------
    void setFontSize(int px) override { fontSize_ = px; }
    void setFontBold(bool bold) override { bold_ = bold; }

    void drawTextAligned(const std::string& text, const Rect& box,
                         Color c, TextAlign align, bool vcenter) override {
        @autoreleasepool {
            NSGraphicsContext* gc = [NSGraphicsContext currentContext];
            [gc saveGraphicsState];

            NSString* s = [NSString stringWithUTF8String:text.c_str()];
            NSColor* color = [NSColor colorWithCalibratedRed:redOf(c)   / 255.0
                                                       green:greenOf(c) / 255.0
                                                        blue:blueOf(c)  / 255.0
                                                       alpha:alphaOf(c) / 255.0];
            NSFont* font = bold_
                ? [NSFont boldSystemFontOfSize:fontSize_]
                : [NSFont systemFontOfSize:fontSize_];
            NSDictionary* attrs = @{
                NSForegroundColorAttributeName : color,
                NSFontAttributeName : font
            };
            NSSize textSize = [s sizeWithAttributes:attrs];

            CGFloat tx = box.x;
            if (align == TextAlign::Center) tx = box.x + (box.w - textSize.width) / 2;
            if (align == TextAlign::Right)  tx = box.x + box.w - textSize.width;

            CGFloat ty = box.y;
            if (vcenter) ty = box.y + (box.h - textSize.height) / 2;

            [s drawAtPoint:NSMakePoint(tx, ty) withAttributes:attrs];

            [gc restoreGraphicsState];
        }
    }

    // ---------- 状态栈 ----------
    void save() override    { CGContextSaveGState(ctx_); }
    void restore() override { CGContextRestoreGState(ctx_); }
    void translate(int dx, int dy) override {
        CGContextTranslateCTM(ctx_, dx, dy);
    }
    
    void clipRect(const Rect& r) override {
        CGContextClipToRect(ctx_, CGRectMake(r.x, r.y, r.w, r.h));
    }

    // ---------- 后端标识 ----------
    Backend backend() const override { return Backend::CG; }

private:
    void setFill(Color c) {
        CGContextSetRGBFillColor(ctx_,
            redOf(c)   / 255.0,
            greenOf(c) / 255.0,
            blueOf(c)  / 255.0,
            alphaOf(c) / 255.0);
    }
    void setStroke(Color c) {
        CGContextSetRGBStrokeColor(ctx_,
            redOf(c)   / 255.0,
            greenOf(c) / 255.0,
            blueOf(c)  / 255.0,
            alphaOf(c) / 255.0);
    }

    CGContextRef ctx_;
    int  fontSize_ = 13;
    bool bold_     = false;
};

// ============================================================
// CocoaWidget
// ============================================================
class CocoaWidget : public Widget {
public:
    CocoaWidget() {
        delegate_ = [[UiWidgetDelegate alloc] init];
        delegate_.owner = this;
    }

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

    // 全堵塞：timeoutMs < 0；-1 是默认值
    bool pumpEvents(int timeoutMs) override {
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

            // 把 pending 的 drawRect 排空（关键：否则 invalidate 不生效）
            if (view_) [view_ displayIfNeeded];
        }
        return !shouldQuit_;
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

    // ---------- 供 ObjC 调用 ----------
    void onPaint(CGContextRef ctx, const Rect& dirty) {
        if (paintCb_) {
            CocoaCanvas canvas(ctx);
            paintCb_(canvas, dirty);
        }
    }

    void onWindowClosed() { shouldQuit_ = true; }

private:
    NSWindow*         window_   = nil;
    UiWidgetView*     view_     = nil;
    UiWidgetDelegate* delegate_ = nil;
    bool shouldQuit_ = false;

    PaintCallback paintCb_;
    EventCallback eventCb_;
};

std::unique_ptr<Widget> createCocoaWidget() {
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