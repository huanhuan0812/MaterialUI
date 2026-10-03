// src/render/canvas.hpp
#pragma once

#include "component/base/types.h"
#include <string>
#include <memory>

namespace ui {

class View;

class Canvas {
public:
    virtual ~Canvas() = default;

    virtual void resize(int width, int height) = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual void fillRect(const Rect& r, Color c) = 0;
    virtual void drawRect(const Rect& r, Color c, int lineWidth = 1) = 0;
    virtual void drawLine(int x0, int y0, int x1, int y1, Color c) = 0;
    virtual void drawText(const std::string& text, int x, int y, Color c) = 0;

    virtual void fillRoundRect(const Rect&, int, Color) = 0;
    virtual void drawRoundRect(const Rect&, int, Color, int = 1) = 0;
    virtual void fillCircle(Point, int, Color) = 0;
    virtual void drawCircle(Point, int, Color, int = 1) = 0;

    virtual void drawShadow(const Rect&, int, int,
                            Color = 0x3C000000) = 0;
    virtual void drawCard(const Rect&, int, Color, int = 1) = 0;

    virtual void fillLinearGradient(const Rect&, Color, Color,
                                    bool vertical = true) = 0;

    enum class TextAlign { Left, Center, Right };
    virtual void drawTextAligned(const std::string&, const Rect&, Color,
                                 TextAlign = TextAlign::Left,
                                 bool vcenter = true) = 0;
    virtual void setFontSize(int) = 0;
    virtual void setFontBold(bool) = 0;

    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void translate(int, int) = 0;
    virtual void clipRect(const Rect&) = 0;

    virtual void drawView(const View& view, int x, int y) = 0;

    enum class Backend { Skia, Metal, Vulkan, D3D, GDI, CG, Unknown };
    virtual Backend backend() const = 0;

    static std::unique_ptr<Canvas> createForPixels(
        uint32_t* pixels, int w, int h);
};

} // namespace ui