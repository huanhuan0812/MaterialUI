// src/render/cocoaCanvas.mm
#include "cocoaCanvas.hpp"

#if defined(__APPLE__)
#include <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#endif

namespace ui {

#if defined(__APPLE__)

struct CocoaCanvas::Impl {
    CGContextRef ctx_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    uint32_t* pixels_ = nullptr;
    CGColorSpaceRef colorSpace_ = nullptr;
    CGDataProviderRef dataProvider_ = nullptr;
    CGImageRef cgImage_ = nullptr;
    int fontSize_ = 13;
    bool bold_ = false;

    void ensureCGImage() {
        if (cgImage_) CGImageRelease(cgImage_);
        CGBitmapInfo info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
        cgImage_ = CGImageCreate(
            width_, height_,
            8, 32, width_ * 4, colorSpace_,
            info, dataProvider_, nullptr, false, kCGRenderingIntentDefault);
    }

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
};

CocoaCanvas::CocoaCanvas(uint32_t* pixels, int width, int height)
    : impl_(std::make_unique<Impl>()) {
    impl_->width_ = width;
    impl_->height_ = height;
    impl_->pixels_ = pixels;
    impl_->colorSpace_ = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
    impl_->dataProvider_ = CGDataProviderCreateWithData(
        nullptr, pixels, width * height * 4, nullptr);
    impl_->cgImage_ = CGImageCreate(
        width, height,
        8, 32, width * 4, impl_->colorSpace_,
        info, impl_->dataProvider_, nullptr, false, kCGRenderingIntentDefault);
    
    CGContextRef ctx = CGBitmapContextCreate(
        pixels, width, height,
        8, width * 4, impl_->colorSpace_, info);
    impl_->ctx_ = ctx;
}

CocoaCanvas::~CocoaCanvas() {
    if (impl_->ctx_) CGContextRelease(impl_->ctx_);
    if (impl_->cgImage_) CGImageRelease(impl_->cgImage_);
    if (impl_->dataProvider_) CGDataProviderRelease(impl_->dataProvider_);
    if (impl_->colorSpace_) CGColorSpaceRelease(impl_->colorSpace_);
}

void CocoaCanvas::resize(int width, int height) {
    impl_->width_ = width;
    impl_->height_ = height;
    impl_->ensureCGImage();
}

int CocoaCanvas::width() const { return impl_->width_; }
int CocoaCanvas::height() const { return impl_->height_; }

void CocoaCanvas::fillRect(const Rect& r, Color c) {
    impl_->setFill(c);
    CGContextFillRect(impl_->ctx_, CGRectMake(r.x, r.y, r.w, r.h));
}

void CocoaCanvas::drawRect(const Rect& r, Color c, int lineWidth) {
    impl_->setStroke(c);
    CGContextSetLineWidth(impl_->ctx_, lineWidth);
    CGContextStrokeRect(impl_->ctx_, CGRectMake(r.x + 0.5, r.y + 0.5,
                                         r.w - 1, r.h - 1));
}

void CocoaCanvas::drawLine(int x0, int y0, int x1, int y1, Color c) {
    impl_->setStroke(c);
    CGContextSetLineWidth(impl_->ctx_, 1);
    CGContextBeginPath(impl_->ctx_);
    CGContextMoveToPoint(impl_->ctx_, x0 + 0.5, y0 + 0.5);
    CGContextAddLineToPoint(impl_->ctx_, x1 + 0.5, y1 + 0.5);
    CGContextStrokePath(impl_->ctx_);
}

void CocoaCanvas::drawText(const std::string& text, int x, int y, Color c) {
    drawTextCoreText(text, x, y, c, false);
}

void CocoaCanvas::drawTextAligned(const std::string& text, const Rect& box,
                                  Color c, TextAlign align, bool vcenter) {
    @autoreleasepool {
        NSString* s = [NSString stringWithUTF8String:text.c_str()];
        NSColor* color = [NSColor colorWithCalibratedRed:redOf(c)   / 255.0
                                                   green:greenOf(c) / 255.0
                                                    blue:blueOf(c)  / 255.0
                                                   alpha:alphaOf(c) / 255.0];
        NSFont* font = impl_->bold_
            ? [NSFont boldSystemFontOfSize:impl_->fontSize_]
            : [NSFont systemFontOfSize:impl_->fontSize_];
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

        drawTextCoreText(text, tx, ty, c, false);
    }
}

void CocoaCanvas::drawTextCoreText(const std::string& text, float x, float y, Color c, bool /*unused*/) {
    if (text.empty()) return;
    
    CGContextRef ctx = impl_->ctx_;
    
    // Create color
    CGColorSpaceRef rgbSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[4] = {
        redOf(c) / 255.0,
        greenOf(c) / 255.0,
        blueOf(c) / 255.0,
        alphaOf(c) / 255.0
    };
    CGColorRef cgColor = CGColorCreate(rgbSpace, components);
    CGColorSpaceRelease(rgbSpace);
    
    // Create CTFont from NSFont
    @autoreleasepool {
        NSFont* nsFont = impl_->bold_
            ? [NSFont boldSystemFontOfSize:impl_->fontSize_]
            : [NSFont systemFontOfSize:impl_->fontSize_];
        CTFontRef ctFont = CTFontCreateWithName((__bridge CFStringRef)nsFont.fontName, impl_->fontSize_, nullptr);
        
        // Create attributed string
        CFStringRef cfString = CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8);
        CFMutableAttributedStringRef attrString = CFAttributedStringCreateMutable(nullptr, 0);
        CFAttributedStringReplaceString(attrString, CFRangeMake(0, 0), cfString);
        CFRelease(cfString);
        
        CFRange fullRange = CFRangeMake(0, text.length());
        CFAttributedStringSetAttribute(attrString, fullRange, kCTFontAttributeName, ctFont);
        CFAttributedStringSetAttribute(attrString, fullRange, kCTForegroundColorAttributeName, cgColor);
        CFRelease(ctFont);
        
        // Create framesetter and frame
        CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrString);
        CFRelease(attrString);
        
        CGMutablePathRef path = CGPathCreateMutable();
        CGPathAddRect(path, nullptr, CGRectMake(0, 0, 10000, 10000)); // Large rect
        CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, nullptr);
        CFRelease(framesetter);
        CFRelease(path);
        
        // Draw text
        CGContextSaveGState(ctx);
        CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
        CGContextTranslateCTM(ctx, x, y);
        // Core Text uses bottom-left origin, CGContext uses top-left in flipped view
        CGContextScaleCTM(ctx, 1.0, -1.0);
        CGContextTranslateCTM(ctx, 0, -impl_->fontSize_);
        CTFrameDraw(frame, ctx);
        CGContextRestoreGState(ctx);
        
        CFRelease(frame);
        CGColorRelease(cgColor);
    }
}

void CocoaCanvas::fillRoundRect(const Rect& r, int radius, Color c) {
    impl_->setFill(c);
    CGPathRef path = CGPathCreateWithRoundedRect(
        CGRectMake(r.x, r.y, r.w, r.h), radius, radius, nullptr);
    CGContextAddPath(impl_->ctx_, path);
    CGContextFillPath(impl_->ctx_);
    CGPathRelease(path);
}

void CocoaCanvas::drawRoundRect(const Rect& r, int radius, Color c, int lw) {
    impl_->setStroke(c);
    CGContextSetLineWidth(impl_->ctx_, lw);
    CGPathRef path = CGPathCreateWithRoundedRect(
        CGRectMake(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1),
        radius, radius, nullptr);
    CGContextAddPath(impl_->ctx_, path);
    CGContextStrokePath(impl_->ctx_);
    CGPathRelease(path);
}

void CocoaCanvas::fillCircle(Point c, int radius, Color col) {
    impl_->setFill(col);
    CGContextFillEllipseInRect(impl_->ctx_,
        CGRectMake(c.x - radius, c.y - radius, radius * 2, radius * 2));
}

void CocoaCanvas::drawCircle(Point c, int radius, Color col, int lw) {
    impl_->setStroke(col);
    CGContextSetLineWidth(impl_->ctx_, lw);
    CGContextStrokeEllipseInRect(impl_->ctx_,
        CGRectMake(c.x - radius + 0.5, c.y - radius + 0.5,
                   radius * 2 - 1, radius * 2 - 1));
}

void CocoaCanvas::drawShadow(const Rect& r, int radius, int elevation,
                             Color shadow) {
    if (elevation <= 0) return;
    CGContextSaveGState(impl_->ctx_);

    CGContextSetShadowWithColor(
        impl_->ctx_,
        CGSizeMake(0, elevation * 0.5),
        elevation * 2.0,
        [NSColor colorWithCalibratedRed:redOf(shadow)   / 255.0
                                  green:greenOf(shadow) / 255.0
                                   blue:blueOf(shadow)  / 255.0
                                  alpha:alphaOf(shadow) / 255.0].CGColor);

    impl_->setFill(shadow);
    CGPathRef path = CGPathCreateWithRoundedRect(
        CGRectMake(r.x, r.y, r.w, r.h), radius, radius, nullptr);
    CGContextAddPath(impl_->ctx_, path);
    CGContextFillPath(impl_->ctx_);
    CGPathRelease(path);

    CGContextRestoreGState(impl_->ctx_);
}

void CocoaCanvas::drawCard(const Rect& r, int radius, Color fill,
                           int elevation) {
    if (elevation > 0) drawShadow(r, radius, elevation);
    fillRoundRect(r, radius, fill);
}

void CocoaCanvas::fillLinearGradient(const Rect& r, Color c0, Color c1,
                                     bool vertical) {
    CGContextSaveGState(impl_->ctx_);

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

    CGContextClipToRect(impl_->ctx_, CGRectMake(r.x, r.y, r.w, r.h));
    CGContextDrawLinearGradient(impl_->ctx_, grad, start, end, 0);

    CGGradientRelease(grad);
    CGColorSpaceRelease(space);
    CGContextRestoreGState(impl_->ctx_);
}

void CocoaCanvas::setFontSize(int px) { impl_->fontSize_ = px; }
void CocoaCanvas::setFontBold(bool bold) { impl_->bold_ = bold; }

void CocoaCanvas::save()    { CGContextSaveGState(impl_->ctx_); }
void CocoaCanvas::restore() { CGContextRestoreGState(impl_->ctx_); }
void CocoaCanvas::translate(int dx, int dy) {
    CGContextTranslateCTM(impl_->ctx_, dx, dy);
}
void CocoaCanvas::clipRect(const Rect& r) {
    CGContextClipToRect(impl_->ctx_, CGRectMake(r.x, r.y, r.w, r.h));
}

void CocoaCanvas::drawView(const View& view, int x, int y) {
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
    
    CGContextSaveGState(impl_->ctx_);
    CGContextTranslateCTM(impl_->ctx_, x, y + view.height());
    CGContextScaleCTM(impl_->ctx_, 1, -1);
    CGRect destRect = CGRectMake(0, 0, view.width(), view.height());
    CGContextDrawImage(impl_->ctx_, destRect, cgImage);
    CGContextRestoreGState(impl_->ctx_);
    
    CGImageRelease(cgImage);
}

#endif

} // namespace ui