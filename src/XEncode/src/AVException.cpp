#include "AVException.h"

extern "C" {
#include <libavutil/error.h>
}

std::string get_error_str(int errcode)
{
    char buf[256] = { 0 };
    av_strerror(errcode, buf, sizeof(buf));
    return buf;
}

//////////////////////////////////////////////////////////////////


AVException::AVException(const std::string &msg, int errcode) :
    std::runtime_error(msg + (errcode ? ": " + get_error_str(errcode) : ""))
{
}
