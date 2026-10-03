// src/render/cocoaCanvas.hpp
#pragma once

#include "canvas.hpp"
#include "view.hpp"
#include <memory>

namespace ui {

class CocoaCanvas : public Canvas {
public:
    CocoaCanvas(uint32_t* pixels, int width, int height);
    ~CocoaCanvas() override;

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

    Backend backend() const override { return Backend::CG; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void drawTextCoreText(const std::string& text, float x, float y, Color c, bool);
};

} // namespace ui