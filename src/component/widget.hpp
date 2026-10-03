#pragma once
#include "core/handle_pool.hpp"
#include "render/view.hpp"
#include "render/canvas.hpp"
#include "component/base/types.h"
#include <memory>
#include <vector>
#include <functional>

namespace ui {

class Widget : public std::enable_shared_from_this<Widget> {
public:
    virtual ~Widget() = default;

    void attach(Handle h, HandlePool<Widget>* pool) {
        handle_ = h;
        pool_   = pool;
    }
    Handle handle() const { return handle_; }

    void setGeometry(const Rect& r) {
        geometry_ = r;
        dirty_ = true;
    }
    Rect geometry() const { return geometry_; }

    View* view() const { return view_.get(); }
    void ensureView(int w, int h) {
        if (!view_) {
            view_ = std::make_unique<View>(w, h, viewBackend_);
            dirty_ = true;
        } else if (view_->width() != w || view_->height() != h) {
            view_->resize(w, h);
            dirty_ = true;
        }
    }
    
    void setViewBackend(ViewBackend backend) {
        viewBackend_ = backend;
        if (view_) {
            // Recreate view with new backend
            view_.reset();
            dirty_ = true;
        }
    }
    ViewBackend viewBackend() const { return viewBackend_; }

    void addChild(std::shared_ptr<Widget> c) {
        if (!c) return;
        c->parent_ = this;
        c->redrawCb_ = redrawCb_;
        children_.push_back(std::move(c));
    }
    const std::vector<std::shared_ptr<Widget>>& children() const {
        return children_;
    }

    virtual void render(Canvas& parentCanvas, int x, int y) {
        ensureView(geometry_.w, geometry_.h);

        onRenderSelf(*view_);

        const int drawX = x + geometry_.x;
        const int drawY = y + geometry_.y;
        parentCanvas.drawView(*view_, drawX, drawY);

        for (auto& c : children_) {
            c->render(parentCanvas, drawX, drawY);
        }
    }

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

    void requestRedraw() {
        dirty_ = true;
        if (redrawCb_) redrawCb_();
    }
    void setRedrawCallback(std::function<void()> cb) {
        redrawCb_ = std::move(cb);
        for (auto& c : children_) c->setRedrawCallback(redrawCb_);
    }

    bool isDirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

protected:
    virtual void onRenderSelf(View& /*v*/) {}
    virtual void onEvent(const Event& /*e*/) {}

    Rect geometry_{0, 0, 0, 0};
    std::vector<std::shared_ptr<Widget>> children_;
    Widget* parent_ = nullptr;

private:
    Handle handle_ = kInvalidHandle;
    HandlePool<Widget>* pool_ = nullptr;
    std::unique_ptr<View> view_;
    std::function<void()> redrawCb_;
    bool dirty_ = true;
    ViewBackend viewBackend_ = ViewBackend::Auto;
};

} // namespace ui