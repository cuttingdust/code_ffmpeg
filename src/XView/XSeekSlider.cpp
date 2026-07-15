#include "XSeekSlider.h"

#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSlider>
#include <QtGui/QMouseEvent>

XSeekSlider::XSeekSlider(QWidget* parent) : QSlider(parent) {}

void XSeekSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || orientation() != Qt::Horizontal || width() <= 0)
    {
        QSlider::mousePressEvent(event);
        return;
    }

    QStyleOptionSlider option;
    initStyleOption(&option);

    const QRect handle_rect =
            style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    if (handle_rect.contains(event->pos()))
    {
        QSlider::mousePressEvent(event);
        return;
    }

    const int value = QStyle::sliderValueFromPosition(minimum(), maximum(), static_cast<int>(event->position().x()),
                                                      width(), option.upsideDown);
    setValue(value);
    emit jumpRequested(value);
    event->accept();
}
