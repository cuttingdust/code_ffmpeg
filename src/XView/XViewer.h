#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>


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

    void contextMenuEvent(QContextMenuEvent *event) override;

    void View(int count);

protected slots:
    void MaxWindow();
    void NormalWindow();
    void View1();
    void View4();
    void View9();
    void View16();

private:
    Ui::XViewerClass *ui_ = nullptr;
    QMenu             left_menu_;
};
