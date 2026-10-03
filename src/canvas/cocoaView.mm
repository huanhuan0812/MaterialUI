#include "render/canvas.hpp"
#include "render/view.hpp"

#include <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

#include <string>

namespace ui {

// ============================================================
// CocoaScreenCanvas (for direct screen rendering)
// ============================================================
class CocoaScreenCanvas : public Canvas {
public:
    explicit CocoaScreenCanvas(CGContextRef ctx) : ctx_(ctx) {}

    // Required by base class (no-op for screen canvas)
    void resize(int, int) override {}
    int width() const override { return 0; }
    int height() const override { return 0; }

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
        fprintf(stderr, "[drawText] text='%s' x=%d y=%d color=0x%08X ctx=%p\n",
            text.c_str(), x, y, c, (void*)ctx_);
        fflush(stderr);
        if (text.empty()) return;

        @autoreleasepool {
            // 构造 CTFont（用 NSFont 拿字体名，保证与系统字体一致）
            NSFont* nsFont = bold_
                ? [NSFont boldSystemFontOfSize:fontSize_]
                : [NSFont systemFontOfSize:fontSize_];
            CTFontRef ctFont = CTFontCreateWithName(
                (__bridge CFStringRef)nsFont.fontName, fontSize_, nullptr);

            // 颜色
            CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
            CGFloat comps[4] = {
                redOf(c)   / 255.0,
                greenOf(c) / 255.0,
                blueOf(c)  / 255.0,
                alphaOf(c) / 255.0
            };
            CGColorRef cgColor = CGColorCreate(rgb, comps);
            CGColorSpaceRelease(rgb);

            // AttributedString
            CFStringRef cfStr = CFStringCreateWithCString(
                nullptr, text.c_str(), kCFStringEncodingUTF8);
            CFMutableAttributedStringRef attr =
                CFAttributedStringCreateMutable(nullptr, 0);
            CFAttributedStringReplaceString(attr, CFRangeMake(0, 0), cfStr);
            CFRelease(cfStr);

            CFRange full = CFRangeMake(0, CFAttributedStringGetLength(attr));
            CFAttributedStringSetAttribute(attr, full,
                kCTFontAttributeName, ctFont);
            CFAttributedStringSetAttribute(attr, full,
                kCTForegroundColorAttributeName, cgColor);
            CFRelease(ctFont);

            // 建 CTLine（单行，定位精确）
            CTLineRef line = CTLineCreateWithAttributedString(attr);
            CFRelease(attr);

            // 度量
            CGFloat ascent = 0, descent = 0, leading = 0;
            CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

            // 绘制：view 是 flipped（左上角原点，y 向下）
            // CoreText 原点在左下、y 向上。
            // 先移到 (x, y + ascent)，再上下翻转，让基线落在 y + ascent。
            CGContextSaveGState(ctx_);
            CGContextSetTextMatrix(ctx_, CGAffineTransformIdentity);
            CGContextTranslateCTM(ctx_, (CGFloat)x, (CGFloat)y + ascent);
            CGContextScaleCTM(ctx_, 1.0, -1.0);
            CTLineDraw(line, ctx_);
            CGContextRestoreGState(ctx_);

            CFRelease(line);
            CGColorRelease(cgColor);
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
                            CGAffineTransform ctm = CGContextGetCTM(ctx_);
    fprintf(stderr, "[drawTextAligned] '%s' box=(%d,%d,%d,%d) CTM=(%g,%g,%g,%g,%g,%g)\n",
            text.c_str(), box.x, box.y, box.w, box.h,
            ctm.a, ctm.b, ctm.c, ctm.d, ctm.tx, ctm.ty);
        if (text.empty()) return;

        @autoreleasepool {
            NSFont* nsFont = bold_
                ? [NSFont boldSystemFontOfSize:fontSize_]
                : [NSFont systemFontOfSize:fontSize_];
            CTFontRef ctFont = CTFontCreateWithName(
                (__bridge CFStringRef)nsFont.fontName, fontSize_, nullptr);

            CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
            CGFloat comps[4] = {
                redOf(c)   / 255.0,
                greenOf(c) / 255.0,
                blueOf(c)  / 255.0,
                alphaOf(c) / 255.0
            };
            CGColorRef cgColor = CGColorCreate(rgb, comps);
            CGColorSpaceRelease(rgb);

            CFStringRef cfStr = CFStringCreateWithCString(
                nullptr, text.c_str(), kCFStringEncodingUTF8);
            CFMutableAttributedStringRef attr =
                CFAttributedStringCreateMutable(nullptr, 0);
            CFAttributedStringReplaceString(attr, CFRangeMake(0, 0), cfStr);
            CFRelease(cfStr);

            CFRange full = CFRangeMake(0, CFAttributedStringGetLength(attr));
            CFAttributedStringSetAttribute(attr, full,
                kCTFontAttributeName, ctFont);
            CFAttributedStringSetAttribute(attr, full,
                kCTForegroundColorAttributeName, cgColor);
            CFRelease(ctFont);

            CTLineRef line = CTLineCreateWithAttributedString(attr);
            CFRelease(attr);

            CGFloat ascent = 0, descent = 0, leading = 0;
            double  width  = CTLineGetTypographicBounds(
                line, &ascent, &descent, &leading);

            CGFloat tx = box.x;
            if (align == TextAlign::Center) tx = box.x + (box.w - width) / 2.0;
            if (align == TextAlign::Right)  tx = box.x + box.w - width;

            // 垂直：文字视觉高度 = ascent + descent
            CGFloat ty = box.y;
            if (vcenter) {
                CGFloat lineH = ascent + descent;
                ty = box.y + (box.h - lineH) / 2.0;
            }

            CGContextSaveGState(ctx_);
            CGContextSetTextMatrix(ctx_, CGAffineTransformIdentity);
            CGContextTranslateCTM(ctx_, tx, ty + ascent);
            CGContextScaleCTM(ctx_, 1.0, -1.0);
            CTLineDraw(line, ctx_);
            CGContextRestoreGState(ctx_);

            CFRelease(line);
            CGColorRelease(cgColor);
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

    void drawView(const View& view, int x, int y) override {
        if (!view.data() || view.width() <= 0 || view.height() <= 0) return;

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGBitmapInfo info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr, view.data(), view.width() * view.height() * 4, nullptr);

        CGImageRef cgImage = CGImageCreate(
            view.width(), view.height(),
            8, 32, view.width() * 4, cs,
            info, provider, nullptr, false, kCGRenderingIntentDefault);

        CGDataProviderRelease(provider);
        CGColorSpaceRelease(cs);

        if (!cgImage) return;

        // View pixels are top-down (Skia convention), but Cocoa expects bottom-up
        // Flip the CTM to draw correctly
        CGContextSaveGState(ctx_);
        // Translate to bottom-left of the image in Cocoa coordinates
        CGContextTranslateCTM(ctx_, x, y + view.height());
        CGContextScaleCTM(ctx_, 1, -1);
        // Draw at origin (0,0) with view size
        CGRect destRect = CGRectMake(0, 0, view.width(), view.height());
        CGContextDrawImage(ctx_, destRect, cgImage);
        CGContextRestoreGState(ctx_);

        CGImageRelease(cgImage);
    }

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