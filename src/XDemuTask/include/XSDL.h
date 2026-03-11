#ifndef XSDL_H
#define XSDL_H

#include "XVideoView.h"

#include <memory>

class XSDL : public XVideoView
{
public:
    XSDL();
    ~XSDL() override;

public:
    auto init(int w, int h, Format fmt) -> bool override;

    auto isExit() -> bool override;

    auto close() -> void override;

    auto draw(const unsigned char *data, int lineSize) -> bool override;

    auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
              int v_pitch) -> bool override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};


#endif // XSDL_H
