#pragma once

#include <QtWidgets/QWidget>

#include <memory>

class LocalPlayer;
class QTimer;

namespace Ui
{
class XPlayClass;
}

class XPlay : public QWidget
{
    Q_OBJECT

public:
    explicit XPlay(QWidget* parent = nullptr);
    ~XPlay() override;

public:
    Ui::XPlayClass* ui = nullptr;

private:
    void initPlayer();

private:
    std::unique_ptr<LocalPlayer> player_;
    QTimer*                      fps_timer_ = nullptr;
};
