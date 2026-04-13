#ifndef XSDL_H
#define XSDL_H

#include "XVideoView.h"

class XSDL : public XVideoView
{
public:
    XSDL();
    ~XSDL() override;
    void setOverlayCallback(OverlayCallback cb) override;

public:
    auto init(int w, int h, Format fmt) -> bool override;
    auto isExit() -> bool override;
    auto close() -> void override;
    auto resetRenderer() -> void override;
    auto draw(const unsigned char *data, int lineSize) -> bool override;
    auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
              int v_pitch) -> bool override;

    void *getSDLRenderer() override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif
