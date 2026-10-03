// src/render/gdiCanvas.hpp
#pragma once

#include "canvas.hpp"
#include "view.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace ui {

#if defined(_WIN32) || defined(_WIN64)
class GDICanvas : public Canvas {
public:
    GDICanvas(uint32_t* pixels, int width, int height);
    ~GDICanvas() override;

    void resize(int width, int height) override;
    int  width()  const override;
    int  height() const override;

    void fillRect(const Rect& r, Color c) override;
    void drawRect(const Rect& r, Color c, int lineWidth = 1) override;
    void drawLine(int x0, int y0, int x1, int y1, Color c) override;
    void drawText(const std::string& text, int x, int y, Color c) override;

    void fillRoundRect(const Rect&, int, Color) override;
    void drawRoundRect(const Rect&, int, Color, int = 1) override;
    void fillCircle(Point, int, Color) override;
    void drawCircle(Point, int, Color, int = 1) override;

    void drawShadow(const Rect&, int, int,
                    Color = 0x3C000000) override;
    void drawCard(const Rect&, int, Color, int = 1) override;

    void fillLinearGradient(const Rect&, Color, Color,
                            bool vertical = true) override;

    void drawTextAligned(const std::string&, const Rect&, Color,
                         TextAlign = TextAlign::Left,
                         bool vcenter = true) override;
    void setFontSize(int) override;
    void setFontBold(bool) override;

    void save() override;
    void restore() override;
    void translate(int, int) override;
    void clipRect(const Rect&) override;

    void drawView(const View& view, int x, int y) override;

    Backend backend() const override { return Backend::GDI; }

private:
    HDC hdc_ = nullptr;
    HBITMAP bmp_ = nullptr;
    HBITMAP oldBmp_ = nullptr;
    int  width_ = 0;
    int  height_ = 0;
    uint32_t* pixels_ = nullptr;
    HFONT font_ = nullptr;
    int  fontSize_ = 13;
    bool bold_ = false;
    bool fontDirty_ = true;
    bool fontSelected_ = false;
    std::vector<int> saved_;

    void applyFont();
    static COLORREF toCOLORREF(Color c);
    static std::wstring toWide(const std::string& s);
};
#endif

} // namespace ui