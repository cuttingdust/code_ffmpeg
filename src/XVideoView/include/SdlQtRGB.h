#ifndef SDLQTRGB_H
#define SDLQTRGB_H

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
    void timerEvent(QTimerEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::SdlQtRGBClass *ui_ = nullptr;
};


#endif // SDLQTRGB_H
