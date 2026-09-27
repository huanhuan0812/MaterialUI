// src/render/skia_canvas.cpp
#include "skia_canvas.hpp"

// ===== 只有这个文件能看到 Skia =====
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkRect.h"
#include "include/core/SkColor.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/effects/SkGradient.h"
#include "include/core/SkSurface.h"

#if defined(__APPLE__)
  #include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
  #include "include/ports/SkTypeface_win.h"
#elif defined(__linux__)
  #include "include/ports/SkFontMgr_fontconfig.h"
#endif

namespace ui {

// ---------------- 辅助函数(文件内静态) ----------------
namespace {

SkColor toSkColor(Color c) {
    return SkColorSetARGB((c >> 24) & 0xFF,
                          (c >> 16) & 0xFF,
                          (c >> 8)  & 0xFF,
                           c        & 0xFF);
}

SkRect toSkRect(const Rect& r) {
    return SkRect::MakeXYWH(SkIntToScalar(r.x), SkIntToScalar(r.y),
                            SkIntToScalar(r.w), SkIntToScalar(r.h));
}

sk_sp<SkFontMgr> defaultFontMgr() {
    static sk_sp<SkFontMgr> mgr = []() -> sk_sp<SkFontMgr> {
#if defined(__APPLE__)
        return SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
        return SkFontMgr_New_DirectWrite();
#elif defined(__linux__)
        return SkFontMgr_New_FontConfig(nullptr);
#else
        return SkFontMgr::RefEmpty();
#endif
    }();
    return mgr;
}

} // namespace

// ---------------- Impl ----------------
struct SkiaCanvasImpl {
    sk_sp<SkSurface> surface;
    SkCanvas*        canvas = nullptr;
    int              width  = 0;
    int              height = 0;

    int  font_size = 14;
    bool bold      = false;

    void ensure(int w, int h) {
        if (surface && w == width && h == height) return;
        width  = w;
        height = h;
        SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
        surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
        canvas  = surface ? surface->getCanvas() : nullptr;
    }

    SkFont makeFont() const {
        SkFont font;
        auto mgr = defaultFontMgr();
        SkFontStyle style = bold ? SkFontStyle::Bold() : SkFontStyle::Normal();
        sk_sp<SkTypeface> tf = mgr ? mgr->matchFamilyStyle(nullptr, style)
                                   : SkTypeface::MakeEmpty();
        if (!tf) tf = SkTypeface::MakeEmpty();
        font.setTypeface(tf);
        font.setSize(SkIntToScalar(font_size));
        font.setEdging(SkFont::Edging::kAntiAlias);
        return font;
    }
};

// ---------------- SkiaCanvas ----------------
SkiaCanvas::SkiaCanvas(int width, int height)
    : impl_(std::make_unique<SkiaCanvasImpl>()) {
    impl_->ensure(width, height);
}

SkiaCanvas::~SkiaCanvas() = default;

void SkiaCanvas::resize(int width, int height) { impl_->ensure(width, height); }
int  SkiaCanvas::width()  const { return impl_->width; }
int  SkiaCanvas::height() const { return impl_->height; }

// 把 SkImage 交给平台目标。这里为了不让 Cocoa 文件碰 Skia,
// 直接在这里做"位图 → CGContext"的桥接(用 void* 接收 CGContextRef)。
void SkiaCanvas::commit(void* target) {
#if defined(__APPLE__)
    if (!target || !impl_->surface) return;
    CGContextRef ctx = static_cast<CGContextRef>(target);

    sk_sp<SkImage> image = impl_->surface->makeImageSnapshot();
    if (!image) return;

    // 从 SkImage 拿像素,构造 CGImage
    SkImageInfo info = image->imageInfo();
    SkPixmap pm;
    if (!image->peekPixels(&pm)) {
        // 拿不到就直接返回(理论上 raster surface 一定能拿到)
        return;
    }

    size_t rowBytes = pm.rowBytes();
    const void* pixels = pm.addr();

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    // Skia N32Premul == BGRA premul on little-endian,对应 CG:
    //   kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
    CGDataProviderRef provider =
        CGDataProviderCreateWithData(nullptr, pixels,
                                     rowBytes * info.height(), nullptr);
    CGImageRef cg = CGImageCreate(info.width(), info.height(),
                                  8, 32, rowBytes, cs,
                                  kCGImageAlphaPremultipliedFirst |
                                  kCGBitmapByteOrder32Little,
                                  provider, nullptr, false,
                                  kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(cs);
    if (!cg) return;

    // CGContext 原点左下,Cocoa flipped 视图里要翻转
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, info.height());
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx,
        CGRectMake(0, 0, info.width(), info.height()), cg);
    CGContextRestoreGState(ctx);

    CGImageRelease(cg);
#else
    (void)target; // 其它平台后续补
#endif
}

// ---------------- Canvas 接口实现 ----------------
void SkiaCanvas::fillRect(const Rect& r, Color c) {
    SkPaint p; p.setColor(toSkColor(c));
    p.setStyle(SkPaint::kFill_Style); p.setAntiAlias(true);
    impl_->canvas->drawRect(toSkRect(r), p);
}
void SkiaCanvas::drawRect(const Rect& r, Color c, int lw) {
    SkPaint p; p.setColor(toSkColor(c));
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(SkIntToScalar(lw)); p.setAntiAlias(true);
    impl_->canvas->drawRect(toSkRect(r), p);
}
void SkiaCanvas::drawLine(int x0, int y0, int x1, int y1, Color c) {
    SkPaint p; p.setColor(toSkColor(c));
    p.setStrokeWidth(1); p.setAntiAlias(true);
    impl_->canvas->drawLine(SkIntToScalar(x0), SkIntToScalar(y0),
                            SkIntToScalar(x1), SkIntToScalar(y1), p);
}
void SkiaCanvas::drawText(const std::string& text, int x, int y, Color c) {
    SkPaint p; p.setColor(toSkColor(c)); p.setAntiAlias(true);
    impl_->canvas->drawString(text.c_str(), x, y, impl_->makeFont(), p);
}

void SkiaCanvas::fillRoundRect(const Rect& r, int radius, Color c) {
    SkPaint p; p.setColor(toSkColor(c));
    p.setStyle(SkPaint::kFill_Style); p.setAntiAlias(true);
    impl_->canvas->drawRoundRect(toSkRect(r),
        SkIntToScalar(radius), SkIntToScalar(radius), p);
}
void SkiaCanvas::drawRoundRect(const Rect& r, int radius, Color c, int lw) {
    SkPaint p; p.setColor(toSkColor(c));
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(SkIntToScalar(lw)); p.setAntiAlias(true);
    impl_->canvas->drawRoundRect(toSkRect(r),
        SkIntToScalar(radius), SkIntToScalar(radius), p);
}
void SkiaCanvas::fillCircle(Point c, int radius, Color col) {
    SkPaint p; p.setColor(toSkColor(col));
    p.setStyle(SkPaint::kFill_Style); p.setAntiAlias(true);
    impl_->canvas->drawCircle(SkIntToScalar(c.x), SkIntToScalar(c.y),
                              SkIntToScalar(radius), p);
}
void SkiaCanvas::drawCircle(Point c, int radius, Color col, int lw) {
    SkPaint p; p.setColor(toSkColor(col));
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(SkIntToScalar(lw)); p.setAntiAlias(true);
    impl_->canvas->drawCircle(SkIntToScalar(c.x), SkIntToScalar(c.y),
                              SkIntToScalar(radius), p);
}

void SkiaCanvas::drawShadow(const Rect& r, int radius, int blur, Color color) {
    SkPaint p; p.setAntiAlias(true); p.setColor(toSkColor(color));
    p.setMaskFilter(SkMaskFilter::MakeBlur(
        SkBlurStyle::kNormal_SkBlurStyle, SkIntToScalar(blur)));
    SkRect sr = toSkRect(r);
    sr.offset(0, SkIntToScalar(blur / 2));
    impl_->canvas->drawRoundRect(sr,
        SkIntToScalar(radius), SkIntToScalar(radius), p);
}
void SkiaCanvas::drawCard(const Rect& r, int radius, Color c, int lw) {
    drawShadow(r, radius, 8, 0x28000000);
    fillRoundRect(r, radius, c);
    if (lw > 0) drawRoundRect(r, radius, 0xFFE0E0E0, lw);
}

void SkiaCanvas::fillLinearGradient(const Rect& r, Color c1, Color c2,
                                    bool vertical) {
    SkPoint pts[2];
    if (vertical) {
        pts[0] = SkPoint::Make(r.x, r.y);
        pts[1] = SkPoint::Make(r.x, r.y + r.h);
    } else {
        pts[0] = SkPoint::Make(r.x, r.y);
        pts[1] = SkPoint::Make(r.x + r.w, r.y);
    }
    SkColor4f colors[2] = {
        SkColor4f::FromColor(toSkColor(c1)),
        SkColor4f::FromColor(toSkColor(c2))
    };
    SkGradient::Colors gradColors(
        SkSpan<const SkColor4f>(colors, 2), SkTileMode::kClamp);
    SkGradient gradient(gradColors, SkGradient::Interpolation{});
    SkPaint p; p.setShader(SkShaders::LinearGradient(pts, gradient));
    p.setAntiAlias(true);
    impl_->canvas->drawRect(toSkRect(r), p);
}

void SkiaCanvas::drawTextAligned(const std::string& text, const Rect& r,
                                 Color c, TextAlign align, bool vcenter) {
    SkPaint p; p.setColor(toSkColor(c)); p.setAntiAlias(true);
    SkFont font = impl_->makeFont();
    SkRect bounds;
    font.measureText(text.c_str(), text.size(),
                     SkTextEncoding::kUTF8, &bounds);

    SkScalar x = SkIntToScalar(r.x);
    if (align == TextAlign::Center) {
        x = SkIntToScalar(r.x) + (SkIntToScalar(r.w) - bounds.width()) / 2
            - bounds.x();
    } else if (align == TextAlign::Right) {
        x = SkIntToScalar(r.x + r.w) - bounds.width() - bounds.x();
    }

    SkScalar y = SkIntToScalar(r.y);
    if (vcenter) {
        SkScalar capHeight = font.getSize() * 0.7f;
        y = SkIntToScalar(r.y) + (SkIntToScalar(r.h) + capHeight) / 2;
    } else {
        y = SkIntToScalar(r.y) + font.getSize();
    }
    impl_->canvas->drawString(text.c_str(), x, y, font, p);
}

void SkiaCanvas::setFontSize(int size) { impl_->font_size = size; }
void SkiaCanvas::setFontBold(bool bold) { impl_->bold = bold; }

void SkiaCanvas::save()    { impl_->canvas->save(); }
void SkiaCanvas::restore() { impl_->canvas->restore(); }
void SkiaCanvas::translate(int dx, int dy) {
    impl_->canvas->translate(SkIntToScalar(dx), SkIntToScalar(dy));
}
void SkiaCanvas::clipRect(const Rect& r) {
    impl_->canvas->clipRect(toSkRect(r));
}

} // namespace ui