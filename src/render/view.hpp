#pragma once
#include "component/base/types.h"
#include <vector>
#include <memory>

namespace ui {

class Canvas;

enum class ViewBackend {
    Auto,
    Skia,
    CG,
    GDI
};

class View {
public:
    View(int w, int h, ViewBackend backend = ViewBackend::Auto);
    ~View();

    int width()  const { return w_; }
    int height() const { return h_; }

    const Rect& rect() const { return rect_; }
    void setRect(const Rect& r) { rect_ = r; }

    uint32_t* data() { return pixels_.data(); }
    const uint32_t* data() const { return pixels_.data(); }

    Canvas& canvas();

    void clear(Color c);

    void resize(int w, int h);

    ViewBackend backend() const { return backend_; }

private:
    int w_, h_;
    Rect rect_{0, 0, 0, 0};
    std::vector<uint32_t> pixels_;
    std::unique_ptr<Canvas> canvas_;
    bool canvasCreated_ = false;
    ViewBackend backend_;
};

} // namespace ui