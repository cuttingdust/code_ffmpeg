#pragma once

#include <QtWidgets/QWidget>

namespace Ui
{
    class TestRGBClass;
}

class RGBTest : public QWidget
{
    Q_OBJECT

public:
    RGBTest(QWidget *parent = Q_NULLPTR);

    /// 重载绘制画面函数
    void paintEvent(QPaintEvent *ev) override;

private:
    Ui::TestRGBClass *ui = nullptr;
};
