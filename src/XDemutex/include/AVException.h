#pragma once

#include <stdexcept>

/// ==================== 异常类 ====================
class AVException : public std::runtime_error
{
public:
    AVException(const std::string& msg, int errcode = 0);
};
