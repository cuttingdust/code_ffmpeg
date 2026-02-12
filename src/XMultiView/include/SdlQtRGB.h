#ifndef SDLQTRGB_H
#define SDLQTRGB_H

#include <thread>

#include <QtWidgets/QWidget>

namespace Ui
{
    class SdlQtRGBClass;
}

class SdlQtRGB : public QWidget
{
    Q_OBJECT
public:
    explicit SdlQtRGB(QWidget *parent = Q_NULLPTR);
    ~SdlQtRGB() override;

public:
    void threadFunc();

    void open(int i);

protected:
    void timerEvent(QTimerEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

signals:
    void signalView();

protected slots:
    void slotView();

    void Open1();

    void Open2();

private:
    Ui::SdlQtRGBClass *ui_ = nullptr;
    std::thread        th_;
    bool               is_exit_ = false; /// 处理线程退出
};


#endif // SDLQTRGB_H
