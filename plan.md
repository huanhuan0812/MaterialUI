# UI 框架完整设计（HandlePool 向后排）

## 一、总体架构

```
┌────────────────────────────────────────────────────────┐
│  Application（驱动引擎）                                │
│  ├─ EventLoop            事件循环                       │
│  ├─ windows_[]           窗口列表                       │
│  └─ createWidget<T>()    直接返回 shared_ptr<T>         │
│                                                        │
│  [延期] HandlePool<Widget>  ← 需要时再加                │
└────────────────────────────────────────────────────────┘
                        │ 驱动
                        ▼
┌────────────────────────────────────────────────────────┐
│  Window（宿主 + 桥接）                                  │
│  ├─ mainView_      主 View（铺满客户区）                │
│  └─ root_          根 Widget（无 View，画到 mainView_） │
└────────────────────────────────────────────────────────┘
                        │ 递归
                        ▼
┌────────────────────────────────────────────────────────┐
│  Widget（逻辑单元）                                     │
│  ├─ id_                  自增 id（延期池的伏笔）         │
│  ├─ geometry_            相对父的位置                   │
│  ├─ view_（可选）        通用绘制容器                   │
│  ├─ children_[]          子控件（shared_ptr 持有）      │
│  ├─ parent_（裸指针）    反向引用（不拥有）             │
│  ├─ render(target,x,y)   坐标累加                       │
│  ├─ dispatchEvent(e)     递归 + 命中测试                │
│  └─ preferredBackend()   声明后端偏好                   │
└────────────────────────────────────────────────────────┘
                        │ 可选持有
                        ▼
┌────────────────────────────────────────────────────────┐
│  View（通用绘制容器 / 最小绘制单元）                    │
│  ├─ backend()            自己的后端类型                 │
│  ├─ canvas()             操作本 View 的 Canvas          │
│  ├─ compositeTo()        跨后端合成入口                 │
│  └─ 实现：CPU / GPU_Texture / NativeLayer / External    │
└────────────────────────────────────────────────────────┘
                        │ GPU 路径
                        ▼
┌────────────────────────────────────────────────────────┐
│  GpuCanvas（2D 批处理、图集、状态排序）                 │
└────────────────────────────────────────────────────────┘
                        │
                        ▼
┌────────────────────────────────────────────────────────┐
│  RHI（渲染硬件接口）                                    │
│  ├─ Device / Queue / CommandBuffer                      │
│  ├─ Texture / Sampler / Pipeline / Buffer               │
│  └─ Swapchain / RenderPass                              │
└────────────────────────────────────────────────────────┘
                        │
     ┌──────────┬───────┼───────┬──────────┐
     ▼          ▼       ▼       ▼          ▼
  OpenGL     D3D11   D3D12   Vulkan     Metal
   GLES
```

---

## 二、核心原则

### 原则 1：View 是通用绘制容器

- 任何 Widget **都可以**持有 View
- View **不是 Layout 专属**
- 是否需要 View 由 Widget **按需决定**

### 原则 2：View 可选、按需创建

| 场景 | 是否建 View |
|---|---|
| 需要裁剪 / 滚动 | ✅ |
| 需要缓存复杂绘制 | ✅ |
| 需要独立透明度 / 变换 | ✅ |
| 需要离屏 / 自定义绘制 | ✅ |
| 需要独立后端 | ✅ |
| 简单直接绘制 | ❌ |
| 纯布局无背景 | ❌ |
| 根 Widget | ❌（mainView_ 兜底） |

### 原则 3：后端是 View 的类型属性

- **View 是什么，后端就是什么**，运行时不可变
- Widget 只**声明偏好**，工厂负责创建和降级
- CPU 是**保底后端**

### 原则 4：6 个图形 API → 必须上 RHI

- OpenGL / GLES / D3D11 / D3D12 / Vulkan / Metal
- 直连 = 6 套代码，不可能维护
- **RHI 是核心基础设施，不是可选优化**

### 原则 5：RHI 是 2D 薄抽象

- 只服务 UI 需求：纹理、管线、命令、交换链
- 不做通用 3D RHI
- 不暴露 barrier / descriptor / 多队列

### 原则 6：生命周期用 shared_ptr，HandlePool 向后排

- **当前**：`shared_ptr`（父子）+ 裸指针（反向引用）
- **伏笔**：Widget 保留自增 `id_`
- **将来**：需要脚本 / 序列化 / 跨进程时，再加 HandlePool

---

## 三、目录结构

```
src/
├── core/
│   ├── widget_id.hpp        【新增】自增 id
│   └── application.hpp/.cpp 【改】去 HandlePool
│
├── render/
│   ├── canvas.hpp           【改】加 drawView()
│   ├── view.hpp             【重写】通用容器 + 多后端
│   ├── view_factory.hpp     【新增】后端工厂
│   └── cpu/
│       ├── cpu_view.hpp
│       └── cpu_canvas.hpp
│
├── rhi/
│   ├── device.hpp
│   ├── texture.hpp
│   ├── pipeline.hpp
│   ├── command_buffer.hpp
│   ├── swapchain.hpp
│   └── backends/
│       ├── metal/
│       ├── vulkan/
│       ├── d3d11/
│       ├── d3d12/
│       ├── gl/
│       └── gles/
│
├── component/
│   ├── widget.hpp           【替换 BaseController】
│   ├── layout.hpp           【新增】
│   ├── button.hpp           【改】继承 Widget
│   ├── scroll_view.hpp      【新增】
│   └── modal.hpp            【新增】
│
├── window/
│   └── window.hpp/.cpp      【改】加 mainView_ + root_
│
└── native/
    └── ...                  【不动】
```

---

## 四、核心模块设计

### ① WidgetId（新增）

**文件**：`src/core/widget_id.hpp`

```cpp
#pragma once
#include <cstdint>

namespace ui {

using WidgetId = uint32_t;
constexpr WidgetId kInvalidWidgetId = 0;

inline WidgetId nextWidgetId() {
    static WidgetId counter = 1;
    return counter++;
}

} // namespace ui
```

- 极简
- 每个 Widget 有稳定 id
- **将来升级 HandlePool 时，id 直接当 handle**

---

### ② Widget（替换 BaseController）

**文件**：`src/component/widget.hpp`

```cpp
#pragma once

#include "core/widget_id.hpp"
#include "render/view.hpp"
#include "types.h"

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
```

**关键点**：

- `id_` 自增，为将来 HandlePool 留伏笔
- `view_` 可选，`unique_ptr` 独占
- `children_` 用 `shared_ptr`，`parent_` 用裸指针，**无循环引用**
- `render` 分两条路径（有 View / 无 View）
- `dispatchEvent` 坐标语义统一

---

### ③ View（重写，通用容器 + 多后端）

**文件**：`src/render/view.hpp`

```cpp
#pragma once

#include "types.h"
#include <memory>

namespace ui {

class Canvas;

class View {
public:
    enum class Backend { CPU, GPU_Texture, NativeLayer, External };

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
```

**实现**：

| 实现 | 后端 | Canvas 类型 |
|---|---|---|
| `CpuBitmapView` | CPU | `CpuCanvas`（软光栅） |
| `GpuTextureView` | GPU_Texture | `GpuCanvas`（2D 批处理） |
| `NativeLayerView` | NativeLayer | `NativeCanvas`（平台 API） |
| `ExternalView` | External | 外部托管 |

---

### ④ ViewFactory（新增）

**文件**：`src/render/view_factory.hpp`

```cpp
#pragma once
#include "view.hpp"
#include <memory>

namespace ui {

class ViewFactory {
public:
    static bool isAvailable(View::Backend b);
    static View::Backend fallback(View::Backend b);
    static std::unique_ptr<View> create(View::Backend b, int w, int h);
};

} // namespace ui
```

**降级链**：

```
GPU_Texture  →  CPU
NativeLayer  →  CPU
External     →  CPU
CPU          →  CPU（保底）
```

---

### ⑤ Canvas（改）

**文件**：`src/render/canvas.hpp`

**新增**：

```cpp
virtual void drawView(View& v, int x, int y) = 0;
// 默认实现：调 v.compositeTo(*this, x, y)
```

其余接口不动。

---

### ⑥ RHI（新增）

**文件**：`src/rhi/*`

**核心对象**：

```cpp
namespace rhi {

class Device;
class Queue;
class Texture;
class Sampler;
class Shader;
class Pipeline;
class Buffer;
class CommandBuffer;
class Swapchain;

}
```

**极简接口**：

```cpp
class Device {
public:
    static std::unique_ptr<Device> create(Backend b);

    Texture*  createTexture(const TextureDesc&);
    Sampler*  createSampler(const SamplerDesc&);
    Shader*   createShader(const ShaderDesc&);
    Pipeline* createPipeline(const PipelineDesc&);
    Buffer*   createBuffer(const BufferDesc&);

    Swapchain*     createSwapchain(void* nativeWindow);
    CommandBuffer* createCommandBuffer();
    Queue& queue();

    void waitIdle();
};

class CommandBuffer {
public:
    void beginRenderPass(Texture* rt, Color clear);
    void setPipeline(Pipeline*);
    void bindTexture(Texture*, Sampler*);
    void setVertexBuffer(Buffer*);
    void setIndexBuffer(Buffer*);
    void draw(uint32_t vertexCount, uint32_t first);
    void drawIndexed(uint32_t indexCount, uint32_t first);
    void endRenderPass();
    void commit();
};

class Queue {
public:
    void submit(CommandBuffer*);
    void present(Swapchain*);
};
```

**统一顶点格式**：

```cpp
struct UIVertex {
    float x, y;         // 屏幕坐标
    float u, v;         // 纹理坐标
    uint32_t color;     // RGBA 顶点色
};
```

**Shader 统一 IR**：一份源码，各后端转译

---

### ⑦ Window（改）

**文件**：`src/window/window.hpp` / `.cpp`

**新增成员**：

```cpp
std::unique_ptr<View> mainView_;
std::shared_ptr<Widget> root_;
WidgetFactory widgetFactory_;
```

**新增接口**：

```cpp
void setWidgetFactory(WidgetFactory f);
Widget* root() const;
void addChild(std::shared_ptr<Widget> c);
```

**create()**：

```cpp
mainView_ = ViewFactory::create(View::Backend::GPU_Texture, w, h);
root_ = widgetFactory_();
root_->setGeometry({0, 0, w, h});
```

**onPaint()**：

```cpp
void Window::onPaint(Canvas& canvas, const Rect& dirty) {
    if (root_) {
        Canvas& mainCanvas = mainView_->canvas();
        mainCanvas.clear(0);
        root_->render(mainCanvas, 0, 0);
        canvas.drawView(*mainView_, 0, 0);
    }
    if (paintHandler_) paintHandler_(canvas, dirty);
}
```

**onEvent()**：

```cpp
void Window::onEvent(const Event& event) {
    if (event.type == EventType::Resize && root_) {
        root_->setGeometry({0, 0, event.width, event.height});
        if (mainView_) mainView_->release();
    }
    if (root_ && root_->dispatchEvent(event)) {
        if (eventHandler_) eventHandler_(event);
        return;
    }
    if (eventHandler_) eventHandler_(event);
}
```

---

### ⑧ Application（改）

**文件**：`src/core/application.hpp` / `.cpp`

**去掉 HandlePool，新增模板方法**：

```cpp
class Application {
public:
    template <typename T, typename... Args>
    std::shared_ptr<T> createWidget(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    Window* createWindow(const std::string& title, int w, int h);
    void    destroyWindow(Window* w);

    int  run();
    void quit();

private:
    std::unique_ptr<EventLoop> loop_;
    std::vector<std::unique_ptr<Window>> windows_;
    std::vector<Window*> pendingDestroy_;
    bool running_ = false;
    bool shouldQuit_ = false;

    // [延期] HandlePool<Widget> widgets_;
};
```

**createWindow()** 注入 WidgetFactory：

```cpp
raw->setWidgetFactory([]() {
    return std::make_shared<Widget>();
});
```

---

### ⑨ Button（改）

**文件**：`src/component/button.hpp`

- 继承 `Widget`
- **默认无 View**（直接画）
- 实现 `onRenderSelf(Canvas& cv)`

```cpp
void Button::onRenderSelf(Canvas& cv) override {
    const Rect r{0, 0, geometry_.w, geometry_.h};
    Color bg = bgNormal_;
    if (pressed_)      bg = bgPressed_;
    else if (hovered_) bg = bgHover_;
    if (radius_ > 0) cv.fillRoundRect(r, radius_, bg);
    else             cv.fillRect(r, bg);
    if (!text_.empty()) {
        cv.setFontSize(fontSize_);
        cv.drawTextAligned(text_, r, textColor_,
                           Canvas::TextAlign::Center, true);
    }
}
```

- 重写 `dispatchEvent` 处理 hover / pressed
- 状态变化时 `requestRedraw()`

---

### ⑩ Layout（新增）

**文件**：`src/component/layout.hpp`

- 继承 `Widget`
- **有 View**
- `preferredBackend()` 返回 `GPU_Texture`
- 可设背景色

```cpp
class Layout : public Widget {
public:
    Layout() { ensureView(0, 0); }
    void setBackground(Color c) { bgColor_ = c; }

protected:
    View::Backend preferredBackend() const override {
        return View::Backend::GPU_Texture;
    }
    void onRenderSelf(Canvas& cv) override {
        if (bgColor_ != 0)
            cv.fillRect({0, 0, geometry_.w, geometry_.h}, bgColor_);
    }
private:
    Color bgColor_ = 0;
};
```

---

### ⑪ ScrollView / Modal（新增）

- 继承 Layout
- ScrollView：视口裁剪 + 内容平移
- Modal：独立图层 + 遮罩

---

## 五、渲染流程

```
Window::onPaint(canvas)
 └─ mainCanvas = mainView_->canvas()
     └─ root_->render(mainCanvas, 0, 0)
         │
         ├─ 无 View 的 Button
         │   └─ mainCanvas.save/translate/onRenderSelf/restore
         │
         └─ 有 View 的 Layout
             ├─ layoutCanvas = layout.view_->canvas()
             ├─ layoutCanvas.clear()
             ├─ Layout::onRenderSelf(layoutCanvas)
             ├─ 子控件 render(layoutCanvas, ...)
             └─ mainCanvas.drawView(*layout.view_, x, y)
                 └─ compositeTo → 跨后端时上传/读回/叠加

 └─ canvas.drawView(*mainView_, 0, 0)
```

---

## 六、事件流程

```
Window::onEvent(e)
 ├─ Resize → 同步 root 几何 + 重建 mainView_
 └─ root_->dispatchEvent(e)
     ├─ 逆序递归子控件
     ├─ hitTest(e.x, e.y)
     ├─ 转局部坐标 → onEvent(local)
     └─ 简单控件直接处理，Layout 继续递归
```

---

## 七、后端选择链

```
Widget::preferredBackend()     声明偏好
        │
        ▼
ViewFactory::isAvailable()     检查可用
        │
        ├─ 可用 → 用偏好
        └─ 不可用 → fallback 降级
        │
        ▼
ViewFactory::create()          创建具体 View
        │
        ▼
View::canvas()                 返回对应后端的 Canvas
```

| Widget | preferredBackend |
|---|---|
| Button / Label | （无 View，无关） |
| Layout / ScrollView | `GPU_Texture` |
| 图表 / 动画 | `GPU_Texture` |
| 视频 / 摄像头 | `NativeLayer` / `External` |
| 截图 / 导出 | `CPU` |
| 单测 | `CPU` |

**降级链**：GPU / NativeLayer / External → **CPU**

---

## 八、跨后端合成规则

| 父 \ 子 | CPU | GPU | NativeLayer |
|---|---|---|---|
| **CPU** | ✅ memcpy | ⚠️ 读回慢 | ❌ 只能叠加 |
| **GPU** | ⚠️ 上传 | ✅ 采样 | ⚠️ 平台相关 |
| **NativeLayer** | ❌ | ❌ | ✅ 平台叠加 |

**实践**：

- 同一子树尽量同后端
- NativeLayer 只做**顶层叠加**
- 跨后端转换要**缓存**
- GPU → CPU 读回尽量避免

---

## 九、RHI 后端实现顺序

```
1. Metal          ← Apple 唯一
2. Vulkan         ← Win/Linux/Android 通用
3. D3D11          ← Windows 保底，实现简单
4. OpenGL / GLES  ← 老平台保底
5. D3D12          ← 最后做（复杂，D3D11 已覆盖 Windows）
```

**Shader 策略**：

- 一份源码（推荐 GLSL 或 HLSL）
- 用 `glslang` / `spirv-cross` 转译到各后端
- 不手写 N 份

---

## 十、刷新（脏矩形）

```
简单控件状态变化 → requestRedraw()
   └─ 冒泡到最近有 View 的祖先（或 Window）
        └─ 标记该 View 脏
             └─ 冒泡到 Window → invalidateAll / invalidate(rect)
```

- 有 Layout 祖先 → 只重绘那块 Layout.View
- 无 → 重绘 Window mainView_

---

## 十一、用户侧用法

```cpp
Application app;
Window* win = app.createWindow("Demo", 400, 300);

// 简单控件（无 View）
auto btn = app.createWidget<Button>("点击我");
btn->setGeometry({140, 130, 120, 40});
btn->setOnClick([](){ /* ... */ });
win->addChild(btn);

// 需要裁剪的 Layout（有 View，GPU）
auto sv = app.createWidget<ScrollView>();
sv->setGeometry({0, 0, 400, 200});
win->addChild(sv);

// 视频（NativeLayer，独立后端）
auto vw = app.createWidget<VideoWidget>();
vw->setGeometry({20, 20, 320, 180});
sv->addChild(vw);

win->show();
return app.run();
```

---

## 十二、HandlePool 延期说明

### 当前做法

- **不建池**
- Widget 用 `shared_ptr` 持有子，`parent_` 用裸指针反向引用
- 每个 Widget 有自增 `id_`

### 延期原因

| 需求 | 当前是否需要 |
|---|---|
| 脚本系统（Lua/JS） | ❌ |
| 序列化 / 存档 | ❌ |
| 网络同步 / 跨进程 | ❌ |
| 全局对象遍历 | ❌ |
| 统一资源管理 | ❌ |

### 触发条件（满足任一即引入）

- 接入脚本系统
- 需要序列化 / 存档
- 需要网络同步 / 跨进程
- 需要全局遍历对象
- 需要弱引用 + 稳定 id 的组合

### 升级路径（将来）

```
1. 新增 HandlePool<Widget>
2. Application 持有池
3. createWidget<T>() 内部：
     make_shared → 池.add() → 返回 handle
4. 加 widget(handle) 查表接口
5. Widget::id_ 直接作为 handle（或映射）
6. 上层代码几乎不动
```

**关键**：`Widget::id_` 已经预留，升级时不需要改 Widget 本身。

---

## 十三、修改顺序

| 步骤 | 内容 | 验证 |
|---|---|---|
| 1 | `WidgetId` + `Widget` 骨架 | 编译通过 |
| 2 | `View` 接口 + `CpuBitmapView` | 能画一块 |
| 3 | `Canvas::drawView` | 能合成 |
| 4 | `Window` + `mainView_` + `root_` | 主 View 能贴 |
| 5 | `Application` + `createWidget<T>()` | 能建 Widget |
| 6 | `Button` 继承 Widget（无 View） | 按钮显示/点击 |
| 7 | `Layout` 基类（有 View） | 布局容器能裁剪 |
| 8 | `ViewFactory` + `preferredBackend` | 后端可切换 |
| 9 | `RHI` 接口 + 单后端（Metal） | GPU 能跑 |
| 10 | `GpuCanvas`（2D 批处理 + 图集） | 2D 命令走 GPU |
| 11 | RHI 加 Vulkan / D3D11 | 跨平台 |
| 12 | RHI 加 OpenGL / GLES / D3D12 | 全覆盖 |
| 13 | `ScrollView` / `Modal` | 滚动/弹层 |
| 14 | 脏矩形 + `requestRedraw` 冒泡 | 局部刷新 |
| 15 | 跨后端合成 + 缓存 | 混合后端跑通 |
| 16 | 打磨（性能、MouseExit、焦点） | 体验完整 |

---

## 十四、核心结论

> **1. View 是通用绘制容器，任何 Widget 可选持有。**
> **2. View 可选、按需创建；简单控件直接画到最近的 View。**
> **3. 后端是 View 的类型属性；Widget 声明偏好，工厂负责创建和降级。**
> **4. 6 个图形 API → 必须上 RHI；RHI 是核心基础设施。**
> **5. RHI 是 2D 薄抽象；统一顶点格式 + 一份 Shader IR。**
> **6. 生命周期用 shared_ptr + 裸指针；HandlePool 向后排，Widget 保留 id_ 伏笔。**
> **7. 跨后端合成走 `compositeTo`；NativeLayer 只能顶层叠加。**
> **8. 实现顺序：CPU → Metal → Vulkan → D3D11 → GL/GLES → D3D12。**

---

## 十五、四条不变式（自检用）

1. **坐标不变式**：`render` 传绝对/累加坐标，`onEvent` 传局部坐标
2. **View 不变式**：有 View 就画进 View 再合成；无 View 就直接画
3. **后端不变式**：View 创建时确定后端，运行时不变；跨后端走 `compositeTo`
4. **所有权不变式**：父持子用 `shared_ptr`，子引父用裸指针；无循环引用

---

## 十六、一句话总结

> **用 `shared_ptr` 管生命周期，用 `id_` 埋 HandlePool 伏笔；View 通用可选，后端随 View；6 个 API 走薄 2D RHI；先 CPU，再 Metal → Vulkan → D3D11 → GL/GLES → D3D12。**

按此设计，从 `WidgetId` + `Widget` 骨架开始逐步落地，每步可编译验证。HandlePool 作为延期项，`id_` 已备好，将来引入时改动可控。