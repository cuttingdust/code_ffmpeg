#pragma once

#include <QtWidgets/QWidget>

#include <memory>

class RtspClient;
class QTimer;

namespace Ui
{
class XRtspDemoClass;
}

class XRtspDemo : public QWidget
{
    Q_OBJECT

public:
    explicit XRtspDemo(QWidget* parent = nullptr);
    ~XRtspDemo() override;

private:
    void initClient();
    void startAutoRecord();
    void updateStatus();

private:
    Ui::XRtspDemoClass*         ui = nullptr;
    std::shared_ptr<RtspClient> client_;
    QTimer*                     status_timer_   = nullptr;
    QTimer*                     record_timer_   = nullptr;
    bool                        record_started_ = false;
};
