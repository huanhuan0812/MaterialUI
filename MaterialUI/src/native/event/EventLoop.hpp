// src/native/event/EventLoop.hpp
#pragma once

#include <memory>

namespace ui {

class NativeWidget;

// ============================================================
// EventLoop：平台事件循环
// ============================================================
class EventLoop {
public:
    virtual ~EventLoop() = default;

    // 绑定主窗口（或任意窗口）；用于判断退出条件
    virtual void attach(NativeWidget* widget) = 0;
    virtual void detach(NativeWidget* widget) = 0;

    // 运行循环，直到 quit() 被调用或绑定窗口关闭
    virtual int run() = 0;

    // 请求退出
    virtual void quit() = 0;

    // 单次步进，便于集成到外部循环
    // timeoutMs < 0 : 无限阻塞
    // timeoutMs == 0: 非阻塞
    // timeoutMs > 0 : 最多等待 timeoutMs 毫秒
    // 返回 false 表示应退出
    virtual bool step(int timeoutMs = -1) = 0;

    // 工厂
    static std::unique_ptr<EventLoop> create();
};

} // namespace ui