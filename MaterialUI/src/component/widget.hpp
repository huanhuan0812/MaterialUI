#pragma once

#include "../core/widgetId.hpp"
#include "../canvas/painter.hpp"
#include "../core/types.h"

#include <memory>
#include <vector>
#include <functional>

namespace ui {

class Widget : public std::enable_shared_from_this<Widget> {
public:
    Widget() : id_(nextWidgetId()) {}
    virtual ~Widget() = default;

    // ========== 身份 ==========
    WidgetId id() const { return id_; }

    // ========== 几何（相对父） ==========
    void setGeometry(const Rect& r) { geometry_ = r; }
    Rect geometry() const { return geometry_; }

    // ========== View（可选） ==========
    View* view() const { return view_.get(); }

    View* ensureView(int w, int h) {
        if (!view_ || view_->width() != w || view_->height() != h) {
            view_ = createView(w, h);
        }
        return view_.get();
    }

    void releaseView() { view_.reset(); }

    // ========== 子控件 ==========
    void addChild(std::shared_ptr<Widget> c) {
        if (!c) return;
        c->parent_ = this;
        children_.push_back(std::move(c));
    }

    const std::vector<std::shared_ptr<Widget>>& children() const {
        return children_;
    }

    Widget* parent() const { return parent_; }

    // ========== 渲染入口 ==========
    virtual void render(Canvas& target, int x, int y) {
        if (view_) {
            // 有 View：画进自己，再合成
            Canvas& my = view_->canvas();
            my.clear(0);
            onRenderSelf(my);
            for (auto& c : children_) {
                c->render(my, c->geometry_.x, c->geometry_.y);
            }
            target.drawView(*view_, x, y);
        } else {
            // 无 View：直接画
            target.save();
            target.translate(x, y);
            onRenderSelf(target);
            for (auto& c : children_) {
                c->render(target, x + c->geometry_.x, y + c->geometry_.y);
            }
            target.restore();
        }
    }

    // ========== 事件 ==========
    virtual bool dispatchEvent(const Event& e) {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if ((*it)->dispatchEvent(e)) return true;
        }

        if (!hitTest(e.x, e.y)) return false;

        Event local = e;
        local.x -= geometry_.x;
        local.y -= geometry_.y;
        onEvent(local);
        return true;
    }

    bool hitTest(int x, int y) const {
        return x >= geometry_.x && x < geometry_.x + geometry_.w &&
               y >= geometry_.y && y < geometry_.y + geometry_.h;
    }

    // ========== 重绘 ==========
    void requestRedraw() {
        if (redrawCb_) redrawCb_();
    }

    void setRedrawCallback(std::function<void()> cb) {
        redrawCb_ = std::move(cb);
    }

protected:
    // ========== 子类钩子 ==========
    virtual void onRenderSelf(Canvas& /*cv*/) {}
    virtual void onEvent(const Event& /*e*/) {}

    virtual View::Backend preferredBackend() const {
        return View::Backend::CPU;
    }

    virtual std::unique_ptr<View> createView(int w, int h) {
        return ViewFactory::create(preferredBackend(), w, h);
    }

private:
    WidgetId id_;
    Rect geometry_{0, 0, 0, 0};
    std::unique_ptr<View> view_;                    // 可选
    std::vector<std::shared_ptr<Widget>> children_; // 正向持有
    Widget* parent_ = nullptr;                      // 反向引用（不拥有）
    std::function<void()> redrawCb_;
};

} // namespace ui
