#include "view.hpp"
#include "canvas.hpp"
#include <algorithm>

#include "skiaCanvas.hpp"

#if defined(__APPLE__)
#include "cocoaCanvas.hpp"
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "gdiCanvas.hpp"
#endif

namespace ui {

View::View(int w, int h, ViewBackend backend)
    : w_(w), h_(h), pixels_(w * h, 0), backend_(backend) {}

View::~View() = default;

Canvas& View::canvas() {
    if (!canvasCreated_) {
        switch (backend_) {
            case ViewBackend::Skia:
                canvas_ = std::make_unique<SkiaCanvas>(pixels_.data(), w_, h_);
                break;
#if defined(__APPLE__)
            case ViewBackend::CG:
                canvas_ = std::make_unique<CocoaCanvas>(pixels_.data(), w_, h_);
                break;
#endif
#if defined(_WIN32) || defined(_WIN64)
            case ViewBackend::GDI:
                canvas_ = std::make_unique<GDICanvas>(pixels_.data(), w_, h_);
                break;
#endif
            case ViewBackend::Auto:
            default:
#if defined(__APPLE__)
                canvas_ = std::make_unique<CocoaCanvas>(pixels_.data(), w_, h_);
#elif defined(_WIN32) || defined(_WIN64)
                canvas_ = std::make_unique<GDICanvas>(pixels_.data(), w_, h_);
#else
                canvas_ = std::make_unique<SkiaCanvas>(pixels_.data(), w_, h_);
#endif
                break;
        }
        canvasCreated_ = true;
    }
    return *canvas_;
}

void View::clear(Color c) {
    std::fill(pixels_.begin(), pixels_.end(), c);
}

void View::resize(int w, int h) {
    if (w == w_ && h == h_) return;
    w_ = w; h_ = h;
    pixels_.assign(w * h, 0);
    canvas_.reset();
    canvasCreated_ = false;
}

} // namespace ui