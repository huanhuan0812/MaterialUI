// src/render/skia_canvas.hpp
#pragma once

#include "canvas.hpp"

// ---- 核心 ----
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkPath.h"
#include "include/core/SkShader.h"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkRect.h"
#include "include/core/SkColor.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkString.h"

// ---- 效果 ----
#include "include/effects/SkGradient.h"

// ---- 平台字体管理器 ----
#if defined(__APPLE__)
  #include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
  #include "include/ports/SkTypeface_win.h"
#elif defined(__linux__)
  #include "include/ports/SkFontMgr_fontconfig.h"
#endif

namespace ui {

class SkiaCanvas : public Canvas {
public:
    explicit SkiaCanvas(SkCanvas* canvas)
        : canvas_(canvas), font_size_(14), bold_(false) {}

    // ---------- 基础图元 ----------
    void fillRect(const Rect& r, Color c) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kFill_Style);
        p.setAntiAlias(true);
        canvas_->drawRect(toSkRect(r), p);
    }

    void drawRect(const Rect& r, Color c, int lineWidth = 1) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(SkIntToScalar(lineWidth));
        p.setAntiAlias(true);
        canvas_->drawRect(toSkRect(r), p);
    }

    void drawLine(int x0, int y0, int x1, int y1, Color c) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStrokeWidth(1);
        p.setAntiAlias(true);
        canvas_->drawLine(SkIntToScalar(x0), SkIntToScalar(y0),
                          SkIntToScalar(x1), SkIntToScalar(y1), p);
    }

    void drawText(const std::string& text, int x, int y, Color c) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setAntiAlias(true);

        SkFont font = makeFont();
        canvas_->drawString(text.c_str(), x, y, font, p);
    }

    // ---------- 圆角 / 圆 ----------
    void fillRoundRect(const Rect& r, int radius, Color c) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kFill_Style);
        p.setAntiAlias(true);
        canvas_->drawRoundRect(toSkRect(r),
                               SkIntToScalar(radius),
                               SkIntToScalar(radius), p);
    }

    void drawRoundRect(const Rect& r, int radius, Color c, int lineWidth = 1) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(SkIntToScalar(lineWidth));
        p.setAntiAlias(true);
        canvas_->drawRoundRect(toSkRect(r),
                               SkIntToScalar(radius),
                               SkIntToScalar(radius), p);
    }

    void fillCircle(Point center, int radius, Color c) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kFill_Style);
        p.setAntiAlias(true);
        canvas_->drawCircle(SkIntToScalar(center.x),
                            SkIntToScalar(center.y),
                            SkIntToScalar(radius), p);
    }

    void drawCircle(Point center, int radius, Color c, int lineWidth = 1) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(SkIntToScalar(lineWidth));
        p.setAntiAlias(true);
        canvas_->drawCircle(SkIntToScalar(center.x),
                            SkIntToScalar(center.y),
                            SkIntToScalar(radius), p);
    }

    // ---------- 阴影 / 卡片 ----------
    void drawShadow(const Rect& r, int radius, int blur,
                    Color color = 0x3C000000) override {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor(toSkColor(color));

        // 新版 API：SkBlurStyle::kNormal + SkMaskFilter::MakeBlur
        p.setMaskFilter(SkMaskFilter::MakeBlur(
            SkBlurStyle::kNormal_SkBlurStyle,
            SkIntToScalar(blur)));

        SkRect shadowRect = toSkRect(r);
        shadowRect.offset(0, SkIntToScalar(blur / 2));

        canvas_->drawRoundRect(shadowRect,
                               SkIntToScalar(radius),
                               SkIntToScalar(radius), p);
    }

    void drawCard(const Rect& r, int radius, Color c, int lineWidth = 1) override {
        drawShadow(r, radius, 8, 0x28000000);
        fillRoundRect(r, radius, c);
        if (lineWidth > 0) {
            drawRoundRect(r, radius, 0xFFE0E0E0, lineWidth);
        }
    }

    // ---------- 渐变 ----------
    void fillLinearGradient(const Rect& r, Color c1, Color c2,
                        bool vertical = true) override {
        SkPoint pts[2];
        if (vertical) {
            pts[0] = SkPoint::Make(SkIntToScalar(r.x), SkIntToScalar(r.y));
            pts[1] = SkPoint::Make(SkIntToScalar(r.x), SkIntToScalar(r.y + r.h));
        } else {
            pts[0] = SkPoint::Make(SkIntToScalar(r.x), SkIntToScalar(r.y));
            pts[1] = SkPoint::Make(SkIntToScalar(r.x + r.w), SkIntToScalar(r.y));
        }

        SkColor4f colors[2] = {
            SkColor4f::FromColor(toSkColor(c1)),
            SkColor4f::FromColor(toSkColor(c2))
        };

        // Colors 构造：colors + tileMode（positions 留空）
        SkGradient::Colors gradColors(
            SkSpan<const SkColor4f>(colors, 2),
            SkTileMode::kClamp);

        // SkGradient 构造：Colors + Interpolation（默认即可）
        SkGradient gradient(gradColors, SkGradient::Interpolation{});

        auto shader = SkShaders::LinearGradient(pts, gradient);

        SkPaint p;
        p.setShader(shader);
        p.setAntiAlias(true);
        canvas_->drawRect(toSkRect(r), p);
    }

    // ---------- 文本 ----------
    void drawTextAligned(const std::string& text, const Rect& r, Color c,
                         TextAlign align = TextAlign::Left,
                         bool vcenter = true) override {
        SkPaint p;
        p.setColor(toSkColor(c));
        p.setAntiAlias(true);

        SkFont font = makeFont();
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

        canvas_->drawString(text.c_str(), x, y, font, p);
    }

    void setFontSize(int size) override { font_size_ = size; }
    void setFontBold(bool bold) override { bold_ = bold; }

    // ---------- 状态栈 ----------
    void save() override { canvas_->save(); }
    void restore() override { canvas_->restore(); }
    void translate(int dx, int dy) override {
        canvas_->translate(SkIntToScalar(dx), SkIntToScalar(dy));
    }
    void clipRect(const Rect& r) override {
        canvas_->clipRect(toSkRect(r));
    }

    Backend backend() const override { return Backend::Skia; }

private:
    SkCanvas* canvas_;
    int font_size_;
    bool bold_;

    // ---------- 辅助函数 ----------
    static SkColor toSkColor(Color c) {
        return SkColorSetARGB(
            (c >> 24) & 0xFF,
            (c >> 16) & 0xFF,
            (c >> 8)  & 0xFF,
            c & 0xFF);
    }

    static SkRect toSkRect(const Rect& r) {
        return SkRect::MakeXYWH(
            SkIntToScalar(r.x),
            SkIntToScalar(r.y),
            SkIntToScalar(r.w),
            SkIntToScalar(r.h));
    }

    // 平台字体管理器（懒加载，进程内只初始化一次）
    static sk_sp<SkFontMgr> defaultFontMgr() {
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

    SkFont makeFont() const {
        SkFont font;

        auto fontMgr = defaultFontMgr();
        SkFontStyle style = bold_ ? SkFontStyle::Bold() : SkFontStyle::Normal();

        sk_sp<SkTypeface> typeface =
            fontMgr ? fontMgr->matchFamilyStyle(nullptr, style)
                    : SkTypeface::MakeEmpty();

        if (!typeface) {
            typeface = SkTypeface::MakeEmpty();
        }

        font.setTypeface(typeface);
        font.setSize(SkIntToScalar(font_size_));
        font.setEdging(SkFont::Edging::kAntiAlias);
        return font;
    }
};

} // namespace ui