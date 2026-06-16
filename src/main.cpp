#include <QApplication>

#include "ducky_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const char* modelPath = argc > 1 ? argv[1] : "model.dky";
    DuckyWindow window(modelPath);
    window.show();

    return app.exec();
}
