#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "view.hpp"

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

#include <string>
#include <memory>
#include <cmath>
#include <vector>
#include <cstdint>

namespace ui {
// ============================================================
// Win32Canvas
// ============================================================
class Win32Canvas : public Canvas {
public:
    explicit Win32Canvas(HDC hdc) : hdc_(hdc) {}
    ~Win32Canvas() override {
        if (font_) DeleteObject(font_);
    }

    // ---------- 基础图元 ----------
    void fillRect(const Rect& r, Color c) override {
        RECT rc{ r.x, r.y, r.x + r.w, r.y + r.h };
        HBRUSH brush = CreateSolidBrush(toCOLORREF(c));
        FillRect(hdc_, &rc, brush);
        DeleteObject(brush);
    }

    void drawRect(const Rect& r, Color c, int lineWidth) override {
        HPEN pen = CreatePen(PS_SOLID, lineWidth, toCOLORREF(c));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        Rectangle(hdc_, r.x, r.y, r.x + r.w, r.y + r.h);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void drawLine(int x0, int y0, int x1, int y1, Color c) override {
        HPEN pen = CreatePen(PS_SOLID, 1, toCOLORREF(c));
        HGDIOBJ oldPen = SelectObject(hdc_, pen);
        MoveToEx(hdc_, x0, y0, nullptr);
        LineTo(hdc_, x1, y1);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void drawText(const std::string& text, int x, int y, Color c) override {
        applyFont();
        SetTextColor(hdc_, toCOLORREF(c));
        SetBkMode(hdc_, TRANSPARENT);
        std::wstring w = toWide(text);
        TextOutW(hdc_, x, y, w.c_str(), static_cast<int>(w.size()));
    }

    // ---------- 圆角 / 圆 ----------
    void fillRoundRect(const Rect& r, int radius, Color c) override {
        HBRUSH brush = CreateSolidBrush(toCOLORREF(c));
        HPEN   pen   = CreatePen(PS_SOLID, 1, toCOLORREF(c));
        HGDIOBJ oldBrush = SelectObject(hdc_, brush);
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        RoundRect(hdc_, r.x, r.y, r.x + r.w, r.y + r.h, radius * 2, radius * 2);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void drawRoundRect(const Rect& r, int radius, Color c, int lw) override {
        HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(c));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        RoundRect(hdc_, r.x, r.y, r.x + r.w, r.y + r.h, radius * 2, radius * 2);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void fillCircle(Point c, int radius, Color color) override {
        HBRUSH brush = CreateSolidBrush(toCOLORREF(color));
        HPEN   pen   = CreatePen(PS_SOLID, 1, toCOLORREF(color));
        HGDIOBJ oldBrush = SelectObject(hdc_, brush);
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        Ellipse(hdc_, c.x - radius, c.y - radius, c.x + radius, c.y + radius);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void drawCircle(Point c, int radius, Color color, int lw) override {
        HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(color));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        Ellipse(hdc_, c.x - radius, c.y - radius, c.x + radius, c.y + radius);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    // ---------- 阴影 ----------
    // GDI 无 alpha 混合，用 AlphaBlend（msimg32）画柔化矩形
    void drawShadow(const Rect& r, int radius, int elevation,
                    Color shadow=0x3C000000) override {
        if (elevation <= 0) return;

        // 逐层半透明叠加，模拟阴影扩散
        BYTE baseA = alphaOf(shadow);
        for (int i = elevation; i >= 1; --i) {
            BYTE a = static_cast<BYTE>(baseA / (i + 1));
            if (a == 0) continue;

            Rect rr{ r.x - i, r.y - i + elevation / 2,
                     r.w + i * 2, r.h + i * 2 };

            // 用 AlphaBlend 画一层柔化圆角矩形
            HDC mem = CreateCompatibleDC(hdc_);
            int w = rr.w, h = rr.h;
            HBITMAP bmp = CreateCompatibleBitmap(hdc_, w, h);
            HGDIOBJ oldBmp = SelectObject(mem, bmp);

            // 画一个实心圆角矩形（颜色 rgb 部分来自 shadow）
            HBRUSH brush = CreateSolidBrush(RGB(redOf(shadow),
                                                greenOf(shadow),
                                                blueOf(shadow)));
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(redOf(shadow),
                                                   greenOf(shadow),
                                                   blueOf(shadow)));
            HGDIOBJ ob = SelectObject(mem, brush);
            HGDIOBJ op = SelectObject(mem, pen);
            RoundRect(mem, 0, 0, w, h, (radius + i) * 2, (radius + i) * 2);
            SelectObject(mem, ob);
            SelectObject(mem, op);
            DeleteObject(brush);
            DeleteObject(pen);

            // 整块用 alpha 混合贴到目标 DC
            BLENDFUNCTION bf{ AC_SRC_OVER, 0, a, 0 };
            AlphaBlend(hdc_, rr.x, rr.y, w, h,
                       mem, 0, 0, w, h, bf);

            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
    }

    void drawCard(const Rect& r, int radius, Color fill,
              int elevation) override {
        if (elevation > 0) drawShadow(r, radius, elevation);  // 靠默认值补 shadow
        fillRoundRect(r, radius, fill);
    }

    // ---------- 渐变 ----------
    void fillLinearGradient(const Rect& r, Color c0, Color c1,
                            bool vertical) override {
        TRIVERTEX v[2] = {
            { r.x, r.y,
              static_cast<COLOR16>(redOf(c0)   << 8),
              static_cast<COLOR16>(greenOf(c0) << 8),
              static_cast<COLOR16>(blueOf(c0)  << 8),
              static_cast<COLOR16>(alphaOf(c0) << 8) },
            { r.x + r.w, r.y + r.h,
              static_cast<COLOR16>(redOf(c1)   << 8),
              static_cast<COLOR16>(greenOf(c1) << 8),
              static_cast<COLOR16>(blueOf(c1)  << 8),
              static_cast<COLOR16>(alphaOf(c1) << 8) }
        };
        GRADIENT_RECT g{ 0, 1 };
        ULONG mode = vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H;
        GradientFill(hdc_, v, 2, &g, 1, mode);
    }

    // ---------- 文本扩展 ----------
    void setFontSize(int px) override {
        if (fontSize_ != px) { fontSize_ = px; fontDirty_ = true; }
    }
    void setFontBold(bool bold) override {
        if (bold_ != bold) { bold_ = bold; fontDirty_ = true; }
    }

    void drawTextAligned(const std::string& text, const Rect& box,
                         Color c, TextAlign align, bool vcenter) override {
        applyFont();
        SetTextColor(hdc_, toCOLORREF(c));
        SetBkMode(hdc_, TRANSPARENT);

        std::wstring w = toWide(text);

        UINT fmt = DT_SINGLELINE | DT_NOPREFIX;
        switch (align) {
            case TextAlign::Left:   fmt |= DT_LEFT;   break;
            case TextAlign::Center: fmt |= DT_CENTER; break;
            case TextAlign::Right:  fmt |= DT_RIGHT;  break;
        }
        fmt |= vcenter ? DT_VCENTER : DT_TOP;

        RECT rc{ box.x, box.y, box.x + box.w, box.y + box.h };
        DrawTextW(hdc_, w.c_str(), static_cast<int>(w.size()), &rc, fmt);
    }

    // ---------- 状态栈 ----------
    void save() override {
        // SaveDC 会保存字体、裁剪、viewport 等全部状态
        saved_.push_back(SaveDC(hdc_));
        // 注意：SaveDC 后当前选中的 font 仍在，但恢复时会还原
    }
    void restore() override {
        if (!saved_.empty()) {
            RestoreDC(hdc_, saved_.back());
            saved_.pop_back();
            // DC 状态被还原，之前选入的 font 可能已失效
            fontSelected_ = false;
        }
    }
    void translate(int dx, int dy) override {
        POINT p{ 0, 0 };
        GetViewportOrgEx(hdc_, &p);
        SetViewportOrgEx(hdc_, p.x + dx, p.y + dy, nullptr);
    }
    void clipRect(const Rect& r) override {
        // 直接传逻辑坐标，GDI 会按当前 viewport 做映射
        // 用 CombineRgn 与现有裁剪区求交更符合直觉
        HRGN rgn = CreateRectRgn(r.x, r.y, r.x + r.w, r.y + r.h);
        // ExtSelectClipRgn(hdc_, rgn, RGN_AND); // 若要与现有求交
        SelectClipRgn(hdc_, rgn);  // RGN_COPY: 替换，配合 save/restore
        DeleteObject(rgn);
    }

    // ---------- 后端标识 ----------
    Backend backend() const override { return Backend::GDI; }

private:
    void applyFont() {
        if (font_ && !fontDirty_ && fontSelected_) return;
        if (font_) DeleteObject(font_);

        LOGFONTW lf{};
        lf.lfHeight  = -fontSize_;
        lf.lfWeight  = bold_ ? FW_BOLD : FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");

        font_ = CreateFontIndirectW(&lf);
        if (font_) {
            // 关键修复：必须 SelectObject 才能生效
            SelectObject(hdc_, font_);
            fontSelected_ = true;
        }
        fontDirty_ = false;
    }

    static COLORREF toCOLORREF(Color c) {
        return RGB(redOf(c), greenOf(c), blueOf(c));
    }
    static std::wstring toWide(const std::string& s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
        return w;
    }

    HDC   hdc_;
    HFONT font_ = nullptr;
    int   fontSize_ = 13;
    bool  bold_ = false;
    bool  fontDirty_ = true;
    bool  fontSelected_ = false;

    std::vector<int> saved_;
};

} // namespace ui

#endif // _WIN32
