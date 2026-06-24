#include <QApplication>

#include "main.hpp"
#include "ducky_window.hpp"

bool g_debug = false;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const char* modelPath = "models/model.dky";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0)
            g_debug = true;
        else
            modelPath = argv[i];
    }
    if (g_debug)
        std::cout << "Debug mode enabled" << std::endl;

    DuckyWindow window(modelPath);
    window.show();

    return app.exec();
}
