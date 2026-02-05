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
    auto init(int w, int h, Format fmt, void *win_id) -> bool override;

    auto isExit() -> bool override;

    auto close() -> void override;

    auto draw(const unsigned char *data, int lineSize) -> bool override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};


#endif // XSDL_H
