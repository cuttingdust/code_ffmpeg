#pragma once

#include <sstream>
#include <string>

enum XLogLevel
{
    XLOG_LEVEL_DEBUG = 0,
    XLOG_LEVEL_INFO  = 1,
    XLOG_LEVEL_WARN  = 2,
    XLOG_LEVEL_ERROR = 3,
    XLOG_LEVEL_FATAL = 4
};

#ifndef LOG_MIN_LEVEL
#define LOG_MIN_LEVEL XLOG_LEVEL_DEBUG
#endif

/// 触发 MLog 单例初始化（可选；首次写日志也会自动初始化）
auto avLogInit() -> void;

auto avLogSetMinLevel(XLogLevel level) -> void;

auto avLogWrite(XLogLevel level, const char* file, int line, const std::string& message) -> void;

#define AVLOG_STREAM(level, stream_expr)                              \
    do                                                                \
    {                                                                 \
        if ((level) >= LOG_MIN_LEVEL)                                 \
        {                                                             \
            std::ostringstream _avlog_stream_;                        \
            _avlog_stream_ << stream_expr;                            \
            avLogWrite((level), __FILE__, __LINE__, _avlog_stream_.str()); \
        }                                                             \
    } while (0)

#define LOGD(s) AVLOG_STREAM(XLOG_LEVEL_DEBUG, s)
#define LOGI(s) AVLOG_STREAM(XLOG_LEVEL_INFO, s)
#define LOGW(s) AVLOG_STREAM(XLOG_LEVEL_WARN, s)
#define LOGE(s) AVLOG_STREAM(XLOG_LEVEL_ERROR, s)
#define LOGF(s) AVLOG_STREAM(XLOG_LEVEL_FATAL, s)

#define LOGD_IF(cond, s) \
    do                   \
    {                    \
        if (cond)        \
            LOGD(s);     \
    } while (0)
#define LOGI_IF(cond, s) \
    do                   \
    {                    \
        if (cond)        \
            LOGI(s);     \
    } while (0)
#define LOGW_IF(cond, s) \
    do                   \
    {                    \
        if (cond)        \
            LOGW(s);     \
    } while (0)
#define LOGE_IF(cond, s) \
    do                   \
    {                    \
        if (cond)        \
            LOGE(s);     \
    } while (0)
