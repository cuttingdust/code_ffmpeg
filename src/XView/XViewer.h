#pragma once

#include <QtWidgets/QWidget>

namespace Ui
{
    class XViewerClass;
}


class XViewer : public QWidget
{
    Q_OBJECT

public:
    XViewer(QWidget *parent = Q_NULLPTR);

private:
    Ui::XViewerClass *ui_ = nullptr;
};
