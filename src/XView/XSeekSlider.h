#pragma once

#include <QtWidgets/QSlider>

class QMouseEvent;

class XSeekSlider : public QSlider
{
    Q_OBJECT

public:
    explicit XSeekSlider(QWidget* parent = nullptr);

signals:
    void jumpRequested(int value);

protected:
    void mousePressEvent(QMouseEvent* event) override;
};
