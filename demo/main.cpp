#include "app/application.hpp"
#include <cstdio>

int main() {
    ui::Application app;
    auto win = app.createWindow("test demo", 800, 600);
    if (!win) {
        std::fprintf(stderr, "create window failed\n");
        return -1;
    }
    win->show();
    return app.run();
}