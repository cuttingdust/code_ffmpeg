#pragma once

#include <QtWidgets/QWidget>

class XCameraWidget : public QWidget
{
    Q_OBJECT
public:
    XCameraWidget(QWidget *p = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void paintEvent(QPaintEvent *event) override;
};
