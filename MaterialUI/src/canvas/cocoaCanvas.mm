#include "canvas.hpp"

#include <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

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

}