#pragma once

#include "XCodec_Global.h"
#include "XRenderBackend.h"
#include "XOverlayStyle.h"

#include <map>
#include <memory>
#include <string>

class QWidget;
class XOpenGLVideoWidget;

enum class PlaybackSpeed : int
{
    SPEED_0_5X = 0,
    SPEED_1_0X = 1,
    SPEED_1_5X = 2,
    SPEED_2_0X = 3,
    SPEED_3_0X = 4,
    SPEED_4_0X = 5,
    SPEED_5_0X = 6
};

class XCODEC_EXPORT LocalPlayer
{
public:
    LocalPlayer();
    ~LocalPlayer();

    bool open(const std::string& filepath, void* winId = nullptr);

    void          setOpenGLWidget(QWidget* widget);
    void          setRenderBackend(RenderBackend backend);
    RenderBackend renderBackend() const;

    void setOverlayStyle(const XOverlayStyle& style);

    void play();
    void pause();
    void resume();
    void stop();

    bool isPlaying() const;
    bool isPaused() const;
    bool isFinished() const;

    void   seek(double seconds);
    void   setSpeed(PlaybackSpeed speed);
    void   setSpeed(double speed);
    double getSpeed() const;

    static std::map<PlaybackSpeed, double> getSupportedSpeeds();

    double getDuration() const;
    double getCurrentTime() const;

    int getWidth() const;
    int getHeight() const;

    bool hasAudio() const;
    void setVolume(double volume);
    double getVolume() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
