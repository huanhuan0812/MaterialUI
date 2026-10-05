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
