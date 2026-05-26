#include <QtWidgets/QApplication>

#include "XRtspDemo.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    XRtspDemo window;
    window.show();

    return app.exec();
}
