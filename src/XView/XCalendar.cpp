
#include "XCalendar.h"
#include <QPainter>
XCalendar::XCalendar(QWidget* p) : QCalendarWidget(p)
{
}
void XCalendar::paintCell(QPainter* painter, const QRect& rec, QDate date) const
{
    /// 有视频的日期特殊显示
    /// 测试日期 4号
    if (date.day() != 4)
    {
        QCalendarWidget::paintCell(painter, rec, date);
        return;
    }
    
    
    auto font = painter->font();
    /// 设置字体
    font.setPixelSize(40);
    
    /// 选中状态刷背景色
    if (date == selectedDate())
    {
        painter->setBrush(QColor(118, 178, 224)); /// 刷子颜色
        painter->drawRect(rec);                   /// 绘制背景
    }
    painter->setFont(font);             /// 设置字体和颜色
    painter->setPen(QColor(255, 0, 0)); /// 字颜色
    painter->drawText(rec, Qt::AlignCenter, QString::number(date.day()));
}
