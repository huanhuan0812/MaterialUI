#pragma once

#include "types.h"
#include "../../render/canvas.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace ui {

class BaseController : public std::enable_shared_from_this<BaseController> {
public:
    virtual ~BaseController() = default;

    // ========== 1. 渲染 ==========
    // 旧接口保留（无参），新接口带 Canvas
    virtual void render() {}
    virtual void render(Canvas& /*cv*/) { render(); }

    // ========== 2. 几何 ==========
    void setGeometry(int x, int y, int w, int h) { geometry_ = {x, y, w, h}; }
    void setGeometry(const Rect& r) { geometry_ = r; }
    Rect getGeometry() const { return geometry_; }

    // ========== 3. 子控件管理 ==========
    void addChild(std::shared_ptr<BaseController> c) {
        children_.push_back(std::move(c));
    }
    void clearChildren() { children_.clear(); }

    const std::vector<std::shared_ptr<BaseController>>& children() const {
        return children_;
    }

    // ========== 4. 事件入口 ==========
    // 参数 e 使用【屏幕（客户区）坐标】，会递归下发给子控件
    // 返回 true 表示事件被消费
    virtual bool dispatchEvent(const Event& e) {
        // 结构性事件不参与命中测试，直接下发
        const bool isStructural =
            e.type == EventType::Resize || e.type == EventType::Close;

        // 键盘事件：交给获得焦点的子控件；此处简化——无焦点系统时
        // 只广播给子控件（先不做拦截）
        if (e.type == EventType::KeyDown ||
            e.type == EventType::KeyUp ||
            e.type == EventType::Char) {
            for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
                if ((*it)->dispatchEvent(e)) return true;
            }
            onEvent(e);
            return true;
        }

        if (!isStructural && !hitTest(e.x, e.y)) return false;

        // 1) 先给子控件（后加的在上层）
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if ((*it)->dispatchEvent(e)) return true;
        }

        // 2) 再给自己：坐标转局部
        Event local = e;
        local.x -= geometry_.x;
        local.y -= geometry_.y;
        onEvent(local);
        return true;
    }

    // ========== 5. 命中测试 ==========
    bool hitTest(int x, int y) const {
        return x >= geometry_.x && x < geometry_.x + geometry_.w &&
               y >= geometry_.y && y < geometry_.y + geometry_.h;
    }

    // ========== 6. 回调 ==========
    void setOnClick(std::function<void()> cb) { onClick_ = std::move(cb); }

protected:
    // 子类重写
    virtual void onEvent(const Event& /*e*/) {}

    // 触发回调
    void fireClick() { if (onClick_) onClick_(); }

    Rect geometry_{0, 0, 0, 0};
    std::vector<std::shared_ptr<BaseController>> children_;

private:
    std::function<void()> onClick_;
};

} // namespace ui