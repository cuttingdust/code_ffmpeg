#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include "XCodec_Global.h"

#include <functional>
#include <memory>
#include <string>

struct AVFrame;

class XCODEC_EXPORT XVideoView
{
public:
    enum Format
    {
        YUV420P = 0,
        NV12    = 23,
        ARGB    = 25,
        RGBA    = 26,
        BGRA    = 28
    };

    enum RenderType
    {
        SDL = 0
    };

    XVideoView();
    virtual ~XVideoView();

    static auto create(RenderType type = SDL) -> XVideoView *;

    using OverlayCallback = std::function<void(void *renderer)>;
    virtual void setOverlayCallback(OverlayCallback cb);

public:
    virtual auto init(int w, int h, Format fmt = RGBA) -> bool             = 0;
    virtual auto close() -> void                                           = 0;
    virtual auto resetRenderer() -> void                                   = 0;
    virtual auto isExit() -> bool                                          = 0;
    virtual auto draw(const unsigned char *data, int lineSize = 0) -> bool = 0;
    virtual auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                      int v_pitch) -> bool                                 = 0;

    auto drawFrame(AVFrame *frame) -> bool;

    /// 获取 SDL 渲染器（仅 XSDL 实现）
    virtual void *getSDLRenderer()
    {
        return nullptr;
    }

    void scale(int w, int h);
    void setScaleWidth(int w);
    void setScaleHeight(int h);
    int  scaleWidth();
    int  scaleHeight();
    int  renderFps() const;

    bool     open(const std::string &filepath);
    AVFrame *read();

    void  setWindow(void *win);
    void *window() const;
    bool  hasWin();

    void   setWidth(int width);
    int    width() const;
    void   setHeight(int height);
    int    height() const;
    void   setFormat(const Format &fmt);
    Format format() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif
