#include "SdlQtRGB.h"

#include <QtWidgets/QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SdlQtRGB     w;
    w.show();
    a.exec();

    return 0;
}
