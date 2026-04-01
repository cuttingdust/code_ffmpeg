#include "XViewer.h"

#include <QtWidgets/QApplication>

#include <iostream>

int main(int argc, char *argv[])
{
    qputenv("QT_LOGGING_RULES", "qt.gui.imageio=false");

    QApplication a(argc, argv);
    XViewer      w;
    w.show();
    return a.exec();
}
