#include "core/application.hpp"
#include "component/button/button.hpp"
#include "render/view.hpp"
#include "component/widget.hpp"

#include <cstdio>

int main() {
    ui::Application app;

    ui::Window* win = app.createWindow("Demo", 400, 300);
    if (!win) {
        std::fprintf(stderr, "create window failed\n");
        return -1;
    }

    // Test with default Auto backend
    ui::Handle btnH = app.createWidget<ui::Button>("Click Me");
    auto btn = std::static_pointer_cast<ui::Button>(app.widget(btnH));

    btn->setGeometry({140, 130, 120, 40});
    btn->setColors(0xFF000000, 0xFF5A90FE, 0xFF2A5AD0);
    btn->setRadius(8);
    btn->setFontSize(16);
    btn->setOnClick([win]() { win->setTitle("Clicked!"); });

    win->addChild(btn);

    win->show();
    return app.run();
}