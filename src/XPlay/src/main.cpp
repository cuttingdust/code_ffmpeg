#include <QtWidgets/QApplication>

#include "XPlay.h"

int main(int argc, char* argv[])
{
    qputenv("QT_LOGGING_RULES", "qt.gui.imageio=false");
    setlocale(LC_ALL, "zh_CN.UTF-8");

    QApplication app(argc, argv);

    XPlay window;
    window.show();

    return app.exec();
}
