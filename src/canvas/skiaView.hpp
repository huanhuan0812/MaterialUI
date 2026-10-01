// src/canvas/skiaView.hpp
#pragma once

#include "view.hpp"
#include <memory>

namespace ui {

// 呈现目标:把 Skia 画完的内容交给平台窗口
// 只暴露 void*,不让上层看到 SkCanvas / CGLContext 等类型
struct SkiaCanvasImpl;   // 前向声明,定义在 .cpp

class SkiaCanvas : public Canvas {
public:
    // 创建一块 W×H 的离屏画布
    SkiaCanvas(int width, int height);
    ~SkiaCanvas() override;

    // 禁止拷贝(内部持有 GPU/像素资源)
    SkiaCanvas(const SkiaCanvas&) = delete;
    SkiaCanvas& operator=(const SkiaCanvas&) = delete;

    // 尺寸变化(会重建底层 surface)
    void resize(int width, int height);
    int  width()  const;
    int  height() const;

    // 把画好的内容提交到某个平台目标
    // target 的语义由后端自己解释:
    //   - macOS: 传 CGContextRef(用 void* 抹掉类型)
    //   - Windows: 传 HDC
    //   - 其它: 传对应句柄
    // 外部不需要知道 Skia 的存在。
    void commit(void* target);

    // ---------- 以下全部是 Canvas 纯虚实现 ----------
    void fillRect(const Rect& r, Color c) override;
    void drawRect(const Rect& r, Color c, int lineWidth = 1) override;
    void drawLine(int x0, int y0, int x1, int y1, Color c) override;
    void drawText(const std::string& text, int x, int y, Color c) override;

    void fillRoundRect(const Rect&, int, Color) override;
    void drawRoundRect(const Rect&, int, Color, int = 1) override;
    void fillCircle(Point, int, Color) override;
    void drawCircle(Point, int, Color, int = 1) override;

    void drawShadow(const Rect&, int, int,
                    Color = 0x3C000000) override;
    void drawCard(const Rect&, int, Color, int = 1) override;

    void fillLinearGradient(const Rect&, Color, Color,
                            bool vertical = true) override;

    void drawTextAligned(const std::string&, const Rect&, Color,
                         TextAlign = TextAlign::Left,
                         bool vcenter = true) override;
    void setFontSize(int) override;
    void setFontBold(bool) override;

    void save() override;
    void restore() override;
    void translate(int, int) override;
    void clipRect(const Rect&) override;

    Backend backend() const override { return Backend::Skia; }

private:
    std::unique_ptr<SkiaCanvasImpl> impl_;
};

} // namespace ui