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

protected:
    bool eventFilter(QObject *pObj, QEvent *pEvent) override;

protected:
    void resizeEvent(QResizeEvent *event) override;

protected slots:
    void MaxWindow();
    void NormalWindow();

private:
    Ui::XViewerClass *ui_ = nullptr;
};
