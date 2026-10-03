#pragma once

#include <algorithm>
#include <cstdint>

namespace ui {

// ==================== 几何 ====================
struct Point { int x = 0, y = 0; };
struct Size  { int w = 0, h = 0; };

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool empty() const { return w <= 0 || h <= 0; }

    int right()  const { return x + w; }
    int bottom() const { return y + h; }

    bool contains(int px, int py) const {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    Rect unionWith(const Rect& o) const {
        if (empty()) return o;
        if (o.empty()) return *this;
        int x0 = std::min(x, o.x);
        int y0 = std::min(y, o.y);
        int x1 = std::max(x + w, o.x + o.w);
        int y1 = std::max(y + h, o.y + o.h);
        return { x0, y0, x1 - x0, y1 - y0 };
    }

    Rect intersect(const Rect& o) const {
        int x0 = std::max(x, o.x);
        int y0 = std::max(y, o.y);
        int x1 = std::min(x + w, o.x + o.w);
        int y1 = std::min(y + h, o.y + o.h);
        if (x1 <= x0 || y1 <= y0) return {};
        return { x0, y0, x1 - x0, y1 - y0 };
    }

    // 脏矩形合并：如果两矩形接近或重叠，合并成一个，减少重绘次数
    bool canMergeWith(const Rect& o, int slack = 8) const {
        if (empty() || o.empty()) return true;
        Rect a{ x - slack, y - slack, w + slack * 2, h + slack * 2 };
        return !a.intersect(o).empty();
    }

    Rect mergedWith(const Rect& o) const { return unionWith(o); }
};

// ==================== 事件 ====================
enum class EventType {
    MouseDown,
    MouseUp,
    MouseMove,
    MouseEnter,   // 鼠标进入控件
    MouseExit,    // 鼠标离开控件
    Wheel,        // 滚轮
    KeyDown,
    KeyUp,
    Char,         // 文本输入（与 KeyDown 区分）
    Resize,
    Close,
};

struct Event {
    EventType type = EventType::MouseMove;

    // 鼠标 / 滚轮坐标（客户区坐标，顶层窗口原点是客户区左上角）
    int x = 0;
    int y = 0;

    // 滚轮：正数向上，负数向下
    int wheelDelta = 0;

    // 键盘：key 为平台原生码（暂不跨平台统一）
    int key = 0;
    uint32_t codepoint = 0;   // Char 事件的 Unicode 码点

    // 修饰键
    bool shift = false;
    bool ctrl  = false;
    bool alt   = false;
    bool meta  = false;       // Cmd / Win 键

    // Resize
    int width  = 0;
    int height = 0;
};

// ==================== 颜色 (0xAARRGGBB) ====================
using Color = uint32_t;

inline Color rgba(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}
inline Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return rgba(0xFF, r, g, b);
}
inline Color hexColor(uint32_t v) { return v | 0xFF000000u; }

inline uint8_t alphaOf(Color c) { return uint8_t(c >> 24); }
inline uint8_t redOf  (Color c) { return uint8_t(c >> 16); }
inline uint8_t greenOf(Color c) { return uint8_t(c >> 8);  }
inline uint8_t blueOf (Color c) { return uint8_t(c);       }

inline Color withAlpha(Color c, uint8_t a) {
    return (uint32_t(a) << 24) | (c & 0x00FFFFFFu);
}

// 简单 alpha 混合：fg over bg
inline Color blend(Color fg, Color bg) {
    uint32_t a = alphaOf(fg);
    if (a == 255) return fg;
    if (a == 0)   return bg;
    auto mix = [&](uint8_t f, uint8_t b) -> uint8_t {
        return uint8_t((f * a + b * (255 - a)) / 255);
    };
    return rgb(mix(redOf(fg),   redOf(bg)),
               mix(greenOf(fg), greenOf(bg)),
               mix(blueOf(fg),  blueOf(bg)));
}

} // namespace ui