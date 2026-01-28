#include "RGBTest.h"

#include <QtWidgets/QApplication>

#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RGBTest      w;
    w.show();
    return a.exec();
}
