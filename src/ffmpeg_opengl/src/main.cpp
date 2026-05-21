#include <QtWidgets/QApplication>
#include "TestOpengl.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TestOpengl window;
    window.show();

    return app.exec();
}
