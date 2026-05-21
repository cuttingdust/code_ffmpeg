#include "TestOpengl.h"
#include "ui_TestQtOpengl.h"
TestOpengl::TestOpengl(QWidget *parent) : QWidget(parent)
{
    ui = new Ui::TestQtOpenglClass();
    ui->setupUi(this);
}

TestOpengl::~TestOpengl()
{
}
