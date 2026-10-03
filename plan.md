# 完整修改方案总结

## 一、总体架构

```
┌──────────────────────────────────────────────────┐
│  Application（驱动引擎）                          │
│  ├─ EventLoop            事件循环                 │
│  ├─ HandlePool<Widget>   控件句柄池               │
│  ├─ HandlePool<View>     视图句柄池（可选）        │
│  └─ windows_[]           窗口列表                 │
└──────────────────────────────────────────────────┘
                     │ 驱动
                     ▼
┌──────────────────────────────────────────────────┐
│  Window（宿主 + 桥接）                            │
│  ├─ mainView_    主 View（铺满客户区）            │
│  └─ root Widget  根控件（无 View，直接画到 mainView）│
└──────────────────────────────────────────────────┘
                     │ 递归
                     ▼
┌──────────────────────────────────────────────────┐
│  Widget（逻辑单元）                               │
│  ├─ geometry_          相对父的位置               │
│  ├─ view_（可选）      通用绘制容器               │
│  ├─ children_[]        子控件                     │
│  ├─ render(target,x,y) 坐标累加                   │
│  ├─ dispatchEvent(e)   递归 + 命中测试            │
│  └─ preferredBackend() 声明后端偏好               │
└──────────────────────────────────────────────────┘
                     │ 可选持有
                     ▼
┌──────────────────────────────────────────────────┐
│  View（通用绘制容器 / 最小绘制单元）              │
│  ├─ backend()          自己的后端类型             │
│  ├─ canvas()           操作本 View 的 Canvas      │
│  ├─ compositeTo()      跨后端合成入口             │
│  └─ 实现：CPU / GPU_Texture / NativeLayer / External│
└──────────────────────────────────────────────────┘
```

---

## 二、三条核心原则

### 原则 1：View 是通用绘制容器

- 任何 Widget **都可以**持有 View
- View 不是 Layout 专属
- 是否需要 View 由 Widget 按需决定

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

- **View 是什么，后端就是什么**，不是运行时可变状态
- Widget 只**声明偏好**，工厂负责创建和降级
- CPU 是**保底后端**，任何平台可用

---

## 三、各模块修改清单

### ① HandlePool（新增）

**文件**：`src/core/handle_pool.hpp`

```cpp
template <typename T>
class HandlePool {
    Handle add(std::shared_ptr<T>);
    void   remove(Handle);
    std::shared_ptr<T> get(Handle) const;
    template <typename Fn> void forEach(Fn&&) const;
    size_t size() const;
};
```

- `Handle = uint32_t`
- 单调递增 id
- 可实例化 `<Widget>` 和 `<View>`

---

### ② View（重写，通用容器 + 多后端）

**文件**：`src/render/view.hpp` / `.cpp`

```cpp
class View {
public:
    enum class Backend { CPU, GPU_Texture, NativeLayer, External };

    virtual ~View() = default;

    virtual Backend backend() const = 0;
    virtual int width()  const = 0;
    virtual int height() const = 0;

    virtual Canvas& canvas() = 0;
    virtual void compositeTo(Canvas& target, int x, int y) = 0;
    virtual void clear(Color c = 0) = 0;
    virtual void release() = 0;
};
```

**具体实现**：

| 实现 | 后端 | Canvas 类型 |
|---|---|---|
| `CpuBitmapView` | CPU | `CpuCanvas`（软光栅） |
| `GpuTextureView` | GPU_Texture | `GpuCanvas`（命令记录） |
| `NativeLayerView` | NativeLayer | `NativeCanvas`（平台 API） |
| `ExternalView` | External | 外部托管 |

---

### ③ ViewFactory（新增）

**文件**：`src/render/view_factory.hpp`

```cpp
class ViewFactory {
public:
    static bool isAvailable(View::Backend b);
    static View::Backend fallback(View::Backend b);
    static std::unique_ptr<View> create(View::Backend b, int w, int h);
};
```

**降级链**：

```
GPU_Texture  →  CPU
NativeLayer  →  CPU
External     →  CPU
CPU          →  CPU（保底）
```

---

### ④ Canvas（改）

**文件**：`src/render/canvas.hpp`

**新增**：

```cpp
virtual void drawView(View& v, int x, int y) = 0;
// 默认实现：调 v.compositeTo(*this, x, y)，由 View 处理跨后端
```

其余接口不动。

---

### ⑤ Widget（替换 BaseController）

**文件**：`src/component/widget.hpp`

**成员**：

```cpp
Handle handle_ = kInvalidHandle;
HandlePool<Widget>* pool_ = nullptr;
Rect geometry_;
std::unique_ptr<View> view_;          // 可选
std::vector<std::shared_ptr<Widget>> children_;
Widget* parent_ = nullptr;
std::function<void()> redrawCb_;
```

**接口**：

```cpp
// 生命周期
void attach(Handle, HandlePool<Widget>*);
Handle handle() const;

// 几何
void setGeometry(const Rect&);
Rect geometry() const;

// View
View* view() const;
View* ensureView(int w, int h);       // 按 preferredBackend 创建
void releaseView();

// 子控件
void addChild(std::shared_ptr<Widget>);
const std::vector<std::shared_ptr<Widget>>& children() const;

// 渲染
virtual void render(Canvas& target, int x, int y);

// 事件
virtual bool dispatchEvent(const Event& e);
bool hitTest(int x, int y) const;

// 重绘
void requestRedraw();
void setRedrawCallback(std::function<void()>);
```

**虚钩子**：

```cpp
protected:
    virtual void onRenderSelf(Canvas& cv) {}   // 统一入口（有无 View 都走这）
    virtual void onEvent(const Event& e) {}    // 局部坐标
    virtual View::Backend preferredBackend() const {
        return View::Backend::CPU;
    }
```

**render 逻辑**：

```cpp
void Widget::render(Canvas& target, int x, int y) {
    if (view_) {
        // 有 View：画进自己，再合成
        Canvas& my = view_->canvas();
        my.clear(0);
        onRenderSelf(my);
        for (auto& c : children_)
            c->render(my, c->geometry_.x, c->geometry_.y);
        target.drawView(*view_, x, y);
    } else {
        // 无 View：直接画
        target.save();
        target.translate(x, y);
        onRenderSelf(target);
        for (auto& c : children_)
            c->render(target, x + c->geometry_.x, y + c->geometry_.y);
        target.restore();
    }
}
```

**dispatchEvent 逻辑**：

```cpp
bool Widget::dispatchEvent(const Event& e) {
    // 逆序递归子控件
    for (auto it = children_.rbegin(); it != children_.rend(); ++it)
        if ((*it)->dispatchEvent(e)) return true;

    // 命中测试
    if (!hitTest(e.x, e.y)) return false;

    // 转局部坐标
    Event local = e;
    local.x -= geometry_.x;
    local.y -= geometry_.y;
    onEvent(local);
    return true;
}
```

**坐标语义**：

| 位置 | 坐标系 |
|---|---|
| `render(target, x, y)` | x/y 是**相对父的绝对坐标** |
| 进入有 View 的 Widget | 坐标系切到**该 View 的局部** |
| 无 View 的 Widget | 只 translate，坐标继续累加 |
| `onEvent` | **局部坐标**（已减 geometry_） |

---

### ⑥ Window（改）

**文件**：`src/window/window.hpp` / `.cpp`

**新增成员**：

```cpp
std::unique_ptr<View> mainView_;         // 主 View
std::shared_ptr<Widget> root_;           // 根控件
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
        if (mainView_) mainView_->release();   // 尺寸变化重建
    }
    if (root_ && root_->dispatchEvent(event)) {
        if (eventHandler_) eventHandler_(event);
        return;
    }
    if (eventHandler_) eventHandler_(event);
}
```

**setSize()**：同步 root 几何 + 重建 mainView_

---

### ⑦ Application（改）

**文件**：`src/core/application.hpp` / `.cpp`

**新增成员**：

```cpp
HandlePool<Widget> widgets_;
```

**新增接口**：

```cpp
template <typename T, typename... Args>
Handle createWidget(Args&&... args);

std::shared_ptr<Widget> widget(Handle h) const;
void destroyWidget(Handle h);
```

**createWindow()** 注入 WidgetFactory：

```cpp
raw->setWidgetFactory([this]() {
    auto root = std::make_shared<Widget>();
    Handle h = widgets_.add(root);
    root->attach(h, &widgets_);
    return root;
});
```

---

### ⑧ Button（改）

**文件**：`src/component/button.hpp`

- 继承 `Widget`
- **默认无 View**（`preferredBackend` 无关紧要，因为不建 View）
- 实现 `onRenderSelf(Canvas& cv)`：

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

- 重写 `dispatchEvent` 处理 hover/pressed
- 状态变化时 `requestRedraw()`

---

### ⑨ Layout 基类（新增）

**文件**：`src/component/layout.hpp`

- 继承 `Widget`
- **有 View**（默认）
- 可设背景色
- `preferredBackend()` 返回 `GPU_Texture`

```cpp
class Layout : public Widget {
public:
    Layout() { ensureView(0, 0); }   // 先占位，render 时按 geometry 重建
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

### ⑩ ScrollView / Modal（新增）

- 继承 Layout
- ScrollView 加视口裁剪 + 内容平移
- Modal 加独立图层 + 遮罩

---

## 四、渲染流程

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
             ├─ Layout::onRenderSelf(layoutCanvas)     // 背景
             ├─ 子控件 render(layoutCanvas, ...)
             └─ mainCanvas.drawView(*layout.view_, x, y)
                 └─ 内部：layout.view_->compositeTo(mainCanvas, x, y)
                     └─ 跨后端时上传/读回/叠加

 └─ canvas.drawView(*mainView_, 0, 0)   // 贴到窗口
```

---

## 五、事件流程

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

## 六、后端选择链

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

**降级链**：GPU/NativeLayer/External → **CPU**（保底）

---

## 七、跨后端合成规则

| 父 \ 子 | CPU | GPU | NativeLayer |
|---|---|---|---|
| **CPU** | ✅ memcpy | ⚠️ 读回慢 | ❌ 只能叠加 |
| **GPU** | ⚠️ 上传 | ✅ 采样 | ⚠️ 平台相关 |
| **NativeLayer** | ❌ | ❌ | ✅ 平台叠加 |

**实践**：

- 同一子树尽量同后端
- NativeLayer 只做**顶层叠加**
- 跨后端转换要**缓存**（上传一次多帧复用）
- GPU → CPU 读回尽量避免

---

## 八、刷新（脏矩形）

```
简单控件状态变化 → requestRedraw()
   └─ 冒泡到最近有 View 的祖先（或 Window）
        └─ 标记该 View 脏
             └─ 冒泡到 Window → invalidateAll / invalidate(rect)
```

- 有 Layout 祖先 → 只重绘那块 Layout.View
- 无 → 重绘 Window mainView_

---

## 九、用户侧用法

```cpp
Application app;
Window* win = app.createWindow("Demo", 400, 300);

// 简单控件（无 View）
Handle h = app.createWidget<Button>("点击我");
auto btn = std::static_pointer_cast<Button>(app.widget(h));
btn->setGeometry({140, 130, 120, 40});
btn->setOnClick([](){ /* ... */ });
win->addChild(btn);

// 需要裁剪的 Layout（有 View，GPU）
Handle lh = app.createWidget<ScrollView>();
auto sv = std::static_pointer_cast<ScrollView>(app.widget(lh));
sv->setGeometry({0, 0, 400, 200});
win->addChild(sv);

// 视频（NativeLayer，独立后端）
Handle vh = app.createWidget<VideoWidget>();
auto vw = std::static_pointer_cast<VideoWidget>(app.widget(vh));
vw->setGeometry({20, 20, 320, 180});
sv->addChild(vw);

win->show();
return app.run();
```

---

## 十、修改顺序

| 步骤 | 内容 | 验证 |
|---|---|---|
| 1 | `HandlePool` | 编译通过 |
| 2 | `View` 接口 + `CpuBitmapView` | 能画一块 |
| 3 | `Canvas::drawView` | 能合成 |
| 4 | `Widget` 骨架（View 可选） | 空窗口渲染 |
| 5 | `Window` + `mainView_` + `root_` | 主 View 能贴 |
| 6 | `Application` + `widgets_` + `createWidget` | 能建 Widget |
| 7 | `Button` 继承 Widget（无 View） | 按钮显示/点击 |
| 8 | `Layout` 基类（有 View） | 布局容器能裁剪 |
| 9 | `ViewFactory` + `preferredBackend` | 后端可切换 |
| 10 | `GpuTextureView` / `NativeLayerView` | 多后端可用 |
| 11 | `ScrollView` / `Modal` | 滚动/弹层 |
| 12 | 脏矩形 + `requestRedraw` 冒泡 | 局部刷新 |
| 13 | 跨后端合成 + 缓存 | 混合后端跑通 |
| 14 | 打磨（性能、MouseExit、焦点） | 体验完整 |

---

## 十一、核心结论

> **1. View 是通用绘制容器，任何 Widget 可选持有。**
> **2. View 可选、按需创建；简单控件直接画到最近的 View。**
> **3. 后端是 View 的类型属性，Widget 声明偏好，工厂负责创建和降级。**
> **4. 按 View 选后端不是额外机制，而是 View 与生俱来的能力。**
> **5. 先做 CPU，接口留好，需要时再加 GPU / NativeLayer。**
> **6. 跨后端合成走 `compositeTo`，NativeLayer 只能顶层叠加。**

---

## 十二、三条不变式（自检用）

1. **坐标不变式**：`render` 传绝对/累加坐标，`onEvent` 传局部坐标
2. **View 不变式**：有 View 就画进 View 再合成；无 View 就直接画
3. **后端不变式**：View 创建时确定后端，运行时不变；跨后端走 `compositeTo`

---

按这个方案逐步替换，每步都能编译验证。先跑通 CPU 单后端，再引入多后端，风险最低。需要细化某一层（如 `CpuCanvas` 实现、跨后端合成细节、ScrollView 裁剪逻辑）时告诉我。