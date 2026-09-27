#include "src/widget/widget.h"
#include <cstdio>

int main() {
    auto w = ui::Widget::create();
    if (!w) { std::fprintf(stderr, "unsupported platform\n"); return 1; }

    // 简单状态
    bool highlighted = false;
    ui::Rect btnRect{ 50, 50, 200, 60 };

    // ---- 绘制回调：只画脏区 ----
    w->setPaintCallback([&](ui::Canvas& c, const ui::Rect& dirty) {
        // 背景：只填脏区
        c.fillRect(dirty, 0xFF2D2D30);

        // 若脏区与按钮相交，才画按钮（脏矩形裁剪的核心思想）
        if (!dirty.intersect(btnRect).empty()) {
            ui::Color bg = highlighted ? 0xFF007ACC : 0xFF3E3E42;
            c.fillRect(btnRect, bg);
            c.drawRect(btnRect, 0xFF6E6E6E);
            c.drawText("Click Me", btnRect.x + 60, btnRect.y + 20, 0xFFFFFFFF);
        }
    });

    // ---- 事件回调 ----
    w->setEventCallback([&](const ui::Event& e) {
        if (e.type == ui::EventType::MouseDown &&
            e.x >= btnRect.x && e.x < btnRect.x + btnRect.w &&
            e.y >= btnRect.y && e.y < btnRect.y + btnRect.h) {
            highlighted = !highlighted;
            w->invalidate(btnRect);   // 只重绘按钮区域
        } else if (e.type == ui::EventType::Close) {
            // close() 已在平台层处理
        }
    });

    if (!w->create("Dirty Rect Demo", 800, 600)) {
        std::fprintf(stderr, "create failed\n");
        return 1;
    }
    w->show();
    w->invalidateAll();

    // ---- 事件驱动主循环 ----
    // 没有事件时阻塞休眠；每 16ms 超时唤醒一次可跑动画。
    while (w->pumpEvents()) {
        // 这里可以放每帧逻辑（如动画）。
        // 若完全不需要周期逻辑，改成 w->pumpEvents() 无限阻塞更省电。
    }

    return 0;
}