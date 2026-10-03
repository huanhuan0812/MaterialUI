// src/render/gdiCanvas.cpp
#include "gdiCanvas.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <windowsx.h>
#endif

namespace ui {

#if defined(_WIN32) || defined(_WIN64)

GDICanvas::GDICanvas(uint32_t* pixels, int width, int height)
    : width_(width), height_(height), pixels_(pixels) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    hdc_ = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    bmp_ = CreateDIBSection(hdc_, &bmi, DIB_RGB_COLORS, (void**)&pixels_, nullptr, 0);
    oldBmp_ = (HBITMAP)SelectObject(hdc_, bmp_);
}

GDICanvas::~GDICanvas() {
    if (hdc_) {
        if (oldBmp_) SelectObject(hdc_, oldBmp_);
        if (bmp_) DeleteObject(bmp_);
        if (font_) DeleteObject(font_);
        DeleteDC(hdc_);
    }
}

void GDICanvas::resize(int width, int height) {
    width_ = width;
    height_ = height;
    if (oldBmp_) SelectObject(hdc_, oldBmp_);
    if (bmp_) DeleteObject(bmp_);
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bmp_ = CreateDIBSection(hdc_, &bmi, DIB_RGB_COLORS, (void**)&pixels_, nullptr, 0);
    oldBmp_ = (HBITMAP)SelectObject(hdc_, bmp_);
    fontDirty_ = true;
    fontSelected_ = false;
}

int GDICanvas::width() const { return width_; }
int GDICanvas::height() const { return height_; }

COLORREF GDICanvas::toCOLORREF(Color c) {
    return RGB(redOf(c), greenOf(c), blueOf(c));
}

std::wstring GDICanvas::toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

void GDICanvas::applyFont() {
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
        SelectObject(hdc_, font_);
        fontSelected_ = true;
    }
    fontDirty_ = false;
}

void GDICanvas::fillRect(const Rect& r, Color c) {
    RECT rc{ r.x, r.y, r.x + r.w, r.y + r.h };
    HBRUSH brush = CreateSolidBrush(toCOLORREF(c));
    FillRect(hdc_, &rc, brush);
    DeleteObject(brush);
}

void GDICanvas::drawRect(const Rect& r, Color c, int lineWidth) {
    HPEN pen = CreatePen(PS_SOLID, lineWidth, toCOLORREF(c));
    HGDIOBJ oldPen   = SelectObject(hdc_, pen);
    HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
    Rectangle(hdc_, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(hdc_, oldBrush);
    SelectObject(hdc_, oldPen);
    DeleteObject(pen);
}

void GDICanvas::drawLine(int x0, int y0, int x1, int y1, Color c) {
    HPEN pen = CreatePen(PS_SOLID, 1, toCOLORREF(c));
    HGDIOBJ oldPen = SelectObject(hdc_, pen);
    MoveToEx(hdc_, x0, y0, nullptr);
    LineTo(hdc_, x1, y1);
    SelectObject(hdc_, oldPen);
    DeleteObject(pen);
}

void GDICanvas::drawText(const std::string& text, int x, int y, Color c) {
    applyFont();
    SetTextColor(hdc_, toCOLORREF(c));
    SetBkMode(hdc_, TRANSPARENT);
    std::wstring w = toWide(text);
    TextOutW(hdc_, x, y, w.c_str(), static_cast<int>(w.size()));
}

void GDICanvas::fillRoundRect(const Rect& r, int radius, Color c) {
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

void GDICanvas::drawRoundRect(const Rect& r, int radius, Color c, int lw) {
    HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(c));
    HGDIOBJ oldPen   = SelectObject(hdc_, pen);
    HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
    RoundRect(hdc_, r.x, r.y, r.x + r.w, r.y + r.h, radius * 2, radius * 2);
    SelectObject(hdc_, oldBrush);
    SelectObject(hdc_, oldPen);
    DeleteObject(pen);
}

void GDICanvas::fillCircle(Point c, int radius, Color color) {
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

void GDICanvas::drawCircle(Point c, int radius, Color color, int lw) {
    HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(color));
    HGDIOBJ oldPen   = SelectObject(hdc_, pen);
    HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
    Ellipse(hdc_, c.x - radius, c.y - radius, c.x + radius, c.y + radius);
    SelectObject(hdc_, oldBrush);
    SelectObject(hdc_, oldPen);
    DeleteObject(pen);
}

void GDICanvas::drawShadow(const Rect& r, int radius, int elevation,
                           Color shadow) {
    if (elevation <= 0) return;

    BYTE baseA = alphaOf(shadow);
    for (int i = elevation; i >= 1; --i) {
        BYTE a = static_cast<BYTE>(baseA / (i + 1));
        if (a == 0) continue;

        Rect rr{ r.x - i, r.y - i + elevation / 2,
                 r.w + i * 2, r.h + i * 2 };

        HDC mem = CreateCompatibleDC(hdc_);
        int w = rr.w, h = rr.h;
        HBITMAP bmp = CreateCompatibleBitmap(hdc_, w, h);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

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

        BLENDFUNCTION bf{ AC_SRC_OVER, 0, a, 0 };
        AlphaBlend(hdc_, rr.x, rr.y, w, h,
                   mem, 0, 0, w, h, bf);

        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
    }
}

void GDICanvas::drawCard(const Rect& r, int radius, Color fill,
                         int elevation) {
    if (elevation > 0) drawShadow(r, radius, elevation);
    fillRoundRect(r, radius, fill);
}

void GDICanvas::fillLinearGradient(const Rect& r, Color c0, Color c1,
                                   bool vertical) {
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

void GDICanvas::drawTextAligned(const std::string& text, const Rect& box,
                                Color c, TextAlign align, bool vcenter) {
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

void GDICanvas::setFontSize(int px) {
    if (fontSize_ != px) { fontSize_ = px; fontDirty_ = true; }
}
void GDICanvas::setFontBold(bool bold) {
    if (bold_ != bold) { bold_ = bold; fontDirty_ = true; }
}

void GDICanvas::save() {
    saved_.push_back(SaveDC(hdc_));
}
void GDICanvas::restore() {
    if (!saved_.empty()) {
        RestoreDC(hdc_, saved_.back());
        saved_.pop_back();
        fontSelected_ = false;
    }
}
void GDICanvas::translate(int dx, int dy) {
    POINT p{ 0, 0 };
    GetViewportOrgEx(hdc_, &p);
    SetViewportOrgEx(hdc_, p.x + dx, p.y + dy, nullptr);
}
void GDICanvas::clipRect(const Rect& r) {
    HRGN rgn = CreateRectRgn(r.x, r.y, r.x + r.w, r.y + r.h);
    SelectClipRgn(hdc_, rgn);
    DeleteObject(rgn);
}

void GDICanvas::drawView(const View& view, int x, int y) {
    if (!view.data() || view.width() <= 0 || view.height() <= 0) return;
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = view.width();
    bmi.bmiHeader.biHeight = -view.height();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    StretchDIBits(hdc_, x, y, view.width(), view.height(),
                  0, 0, view.width(), view.height(),
                  view.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

#endif

} // namespace ui