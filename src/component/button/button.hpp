#pragma once
#include "component/widget.hpp"
#include <string>
#include <functional>

namespace ui {

class Button : public Widget {
public:
    Button() = default;
    explicit Button(std::string text) : text_(std::move(text)) {}

    void setText(std::string t) { text_ = std::move(t); requestRedraw(); }
    const std::string& text() const { return text_; }

    void setColors(Color bg, Color hover, Color pressed,
                   Color textColor = 0xFFFFFFFF) {
        bgNormal_ = bg; bgHover_ = hover; bgPressed_ = pressed;
        textColor_ = textColor;
        requestRedraw();
    }
    void setRadius(int r)          { radius_ = r; requestRedraw(); }
    void setFontSize(int s)        { fontSize_ = s; requestRedraw(); }
    void setBorder(Color c, int w) { borderColor_ = c; borderWidth_ = w; requestRedraw(); }

    void setOnClick(std::function<void()> cb) { onClick_ = std::move(cb); }

    bool dispatchEvent(const Event& e) override {
        const bool inside = hitTest(e.x, e.y);

        switch (e.type) {
        case EventType::MouseMove:
            if (inside != hovered_) { hovered_ = inside; requestRedraw(); }
            return inside;

        case EventType::MouseDown:
            if (!inside) return false;
            if (!pressed_) { pressed_ = true; requestRedraw(); }
            return true;

        case EventType::MouseUp:
            if (pressed_) {
                pressed_ = false;
                requestRedraw();
                if (inside && onClick_) onClick_();
                return true;
            }
            return false;

        case EventType::MouseExit:
            if (hovered_ || pressed_) {
                hovered_ = false; pressed_ = false;
                requestRedraw();
            }
            return false;

        default:
            return Widget::dispatchEvent(e);
        }
    }

protected:
    void onRenderSelf(View& v) override {
        fprintf(stderr, "[Button::onRenderSelf] text='%s' size=(%d,%d) color=0x%08X\n",
            text_.c_str(), v.width(), v.height(), textColor_);
    fflush(stderr);
        Canvas& cv = v.canvas();
        fprintf(stderr, "[Button] backend=%d width=%d height=%d text='%s'\n",
            (int)cv.backend(), cv.width(), cv.height(), text_.c_str());
    fflush(stderr);
        const Rect r{0, 0, v.width(), v.height()};

        Color bg = bgNormal_;
        if (pressed_)      bg = bgPressed_;
        else if (hovered_) bg = bgHover_;

        if (radius_ > 0) cv.fillRoundRect(r, radius_, bg);
        else             cv.fillRect(r, bg);

        if (borderWidth_ > 0 && borderColor_ != 0) {
            if (radius_ > 0) cv.drawRoundRect(r, radius_, borderColor_, borderWidth_);
            else             cv.drawRect(r, borderColor_, borderWidth_);
        }

        if (!text_.empty()) {
            cv.setFontSize(fontSize_);
            cv.drawTextAligned(text_, r, textColor_,
                               Canvas::TextAlign::Center, true);
        }
    }

private:
    std::string text_;
    Color bgNormal_  = 0xFF3A7AFE;
    Color bgHover_   = 0xFF5A90FE;
    Color bgPressed_ = 0xFF2A5AD0;
    Color textColor_ = 0xFFFFFFFF;
    Color borderColor_ = 0;
    int   borderWidth_ = 0;
    int   radius_ = 6;
    int   fontSize_ = 14;
    bool  hovered_ = false;
    bool  pressed_ = false;
    std::function<void()> onClick_;
};

} // namespace ui