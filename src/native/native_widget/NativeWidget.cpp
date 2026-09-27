// src/native/native_widget/widget.cpp
#include "NativeWidget.h"

#if defined(_WIN32)
namespace ui { std::unique_ptr<Widget> createWin32Widget(); }
#elif defined(__APPLE__)
namespace ui { std::unique_ptr<Widget> createCocoaWidget(); }
#endif

namespace ui {

std::unique_ptr<Widget> Widget::create() {
#if defined(_WIN32)
    return createWin32Widget();
#elif defined(__APPLE__)
    return createCocoaWidget();
#else
    return nullptr;
#endif
}

} // namespace ui