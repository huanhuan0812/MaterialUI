#pragma once

#include "../core/types.h"
#include <memory>

namespace ui {

class Canvas;

class View {
public:
    enum class Backend { CPU, GPU, NativeLayer, External };
    /*
    * CPU: 使用 CPU 进行渲染 （fallback）
    * GPU: 使用 GPU 进行渲染(2D)(默认)
    * NativeLayer: 使用原生层进行渲染
    * External: 使用外部库进行渲染(摄像头、视频、OpenGL 等)
    */

    virtual ~View() = default;

    virtual Backend backend() const = 0;
    virtual int width()  const = 0;
    virtual int height() const = 0;

    virtual Canvas& canvas() = 0;
    virtual void clear(Color c = 0) = 0;
    virtual void compositeTo(Canvas& target, int x, int y) = 0;
    virtual void release() = 0;
};

} // namespace ui
