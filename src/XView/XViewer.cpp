#include "XViewer.h"

#include "ui_xviewer.h"


XViewer::XViewer(QWidget *parent) : QWidget(parent)
{
    ui_ = new Ui::XViewerClass;
    ui_->setupUi(this);
}
