#pragma once
#include <QCalendarWidget>
class XCalendar : public QCalendarWidget
{
public:
    XCalendar(QWidget* p);
    void paintCell(QPainter* painter, const QRect& rec, QDate date) const override;
};
