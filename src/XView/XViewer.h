#pragma once

#include <QtWidgets/QWidget>
#include <QMenu>

namespace Ui
{
    class XViewerClass;
}

class XViewer : public QWidget
{
    Q_OBJECT

public:
    explicit XViewer(QWidget *parent = nullptr);
    ~XViewer();

protected:
    bool eventFilter(QObject *pObj, QEvent *pEvent) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void View1();
    void View4();
    void View9();
    void View16();
    void MaxWindow();
    void NormalWindow();
    void AddCam();
    void SetCam();
    void DelCam();

private:
    void view(int count);
    void refreshCameras();
    void updateCam(int index);

private:
    Ui::XViewerClass *ui;
    QMenu             left_menu_;
};
