#pragma once

#include "../base/baseControler.hpp"
#include "../../render/canvas.hpp"
#include "../base/types.h"
#include <string>

namespace ui {

class Button : public BaseController {
public:
    // ---------- 构造 ----------
    Button() = default;
    explicit Button(std::string text) : text_(std::move(text)) {}

    // ---------- 文本 ----------
    void setText(std::string t) { text_ = std::move(t); }
    const std::string& text() const { return text_; }

    // ---------- 外观 ----------
    void setColors(Color bg, Color hover, Color pressed,
                   Color textColor = 0xFFFFFFFF) {
        bgNormal_  = bg;
        bgHover_   = hover;
        bgPressed_ = pressed;
        textColor_ = textColor;
    }

    void setRadius(int r)        { radius_    = r; }
    void setBorder(Color c, int w) { borderColor_ = c; borderWidth_ = w; }
    void setFontSize(int s)      { fontSize_   = s; }

    // 用 setOnClick() 注册点击回调（继承自 BaseController）

    // ---------- 渲染 ----------
    void render(Canvas& cv) override {
        const Rect r = getGeometry();
        Color bg = bgNormal_;
        if (pressed_)      bg = bgPressed_;
        else if (hovered_) bg = bgHover_;

        // 背景
        if (radius_ > 0)
            cv.fillRoundRect(r, radius_, bg);
        else
            cv.fillRect(r, bg);

        // 边框
        if (borderWidth_ > 0 && borderColor_ != 0) {
            if (radius_ > 0)
                cv.drawRoundRect(r, radius_, borderColor_, borderWidth_);
            else
                cv.drawRect(r, borderColor_, borderWidth_);
        }

        // 文本
        if (!text_.empty()) {
            cv.setFontSize(fontSize_);
            cv.drawTextAligned(text_, r, textColor_,
                               Canvas::TextAlign::Center, /*vcenter=*/true);
        }

        // 子控件（如果有）
        for (auto& c : children_) c->render(cv);
    }

protected:
    // ---------- 事件处理 ----------
    // 注意：dispatchEvent 已经做过 hitTest，并把坐标转成局部坐标再调用 onEvent
    // 但按钮需要区分“按下/移动/抬起”，所以这里最好用全局坐标——因此建议
    // 重写 dispatchEvent，或者在此处基于 local 坐标做简单判断。
    // 为简洁起见，这里直接重写 dispatchEvent。
    void onEvent(const Event& /*e*/) override {
        // 不使用：改由 dispatchEvent 处理
    }

public:
    bool dispatchEvent(const Event& e) override {
        const bool inside = hitTest(e.x, e.y);

        switch (e.type) {
        case EventType::MouseMove:
            hovered_ = inside;
            return inside;   // 悬停到按钮上才算消费

        case EventType::MouseDown:
            if (!inside) return false;
            pressed_ = true;
            // 捕获：即使鼠标移出也继续接收 MouseUp
            // 简化处理：不实现鼠标捕获，靠下面的 MouseUp 判 inside
            return true;

        case EventType::MouseUp:
            if (pressed_) {
                const bool wasPressed = pressed_;
                pressed_ = false;
                if (inside && wasPressed) {
                    fireClick();     // 触发 onClick_ 回调
                }
                return true;
            }
            return false;

        case EventType::MouseExit:
            hovered_ = false;
            pressed_ = false;
            return false;

        default:
            // 键盘等事件交给基类（会下发给子控件）
            return BaseController::dispatchEvent(e);
        }
    }

private:
    std::string text_;

    Color bgNormal_  = 0xFF3A7AFE;  // 蓝
    Color bgHover_   = 0xFF5A90FE;
    Color bgPressed_ = 0xFF2A5AD0;
    Color textColor_ = 0xFFFFFFFF;

    Color borderColor_ = 0;
    int   borderWidth_ = 0;
    int   radius_      = 6;
    int   fontSize_    = 14;

    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace ui