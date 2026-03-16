#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

// 日志级别
enum XLogLevel
{
    XLOG_LEVEL_DEBUG = 0,
    XLOG_LEVEL_INFO  = 1,
    XLOG_LEVEL_WARN  = 2, // ✅ 新增警告级别
    XLOG_LEVEL_ERROR = 3,
    XLOG_LEVEL_FATAL = 4
};

// 默认日志级别，可以在编译时修改
#ifndef LOG_MIN_LEVEL
#define LOG_MIN_LEVEL XLOG_LEVEL_DEBUG
#endif

/// 获取当前时间字符串
inline std::string getCurrentTimeString()
{
    auto now       = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms        = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm bt;
    localtime_s(&bt, &in_time_t);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &bt);

    char timebuf[80];
    sprintf_s(timebuf, "%s.%03lld", buf, ms.count());

    return std::string(timebuf);
}

/// 获取日志级别字符串
inline const char* getLevelString(XLogLevel level)
{
    switch (level)
    {
        case XLOG_LEVEL_DEBUG:
            return "DEBUG";
        case XLOG_LEVEL_INFO:
            return "INFO ";
        case XLOG_LEVEL_WARN: // ✅ 新增
            return "WARN ";
        case XLOG_LEVEL_ERROR:
            return "ERROR";
        case XLOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

/// 基础日志宏
#define AVLOG(s, level)                                \
    if (level >= LOG_MIN_LEVEL)                        \
    std::cout << "[" << getCurrentTimeString() << "] " \
              << "[" << getLevelString(level) << "] "  \
              << "[" << __FILE__ << ":" << __LINE__ << "] " << s << std::endl

/// 简化的日志宏
#define LOGD(s) AVLOG(s, XLOG_LEVEL_DEBUG)
#define LOGI(s) AVLOG(s, XLOG_LEVEL_INFO)
#define LOGW(s) AVLOG(s, XLOG_LEVEL_WARN) // ✅ 新增警告宏
#define LOGE(s) AVLOG(s, XLOG_LEVEL_ERROR)
#define LOGF(s) AVLOG(s, XLOG_LEVEL_FATAL)

/// 条件日志宏（调试用）
#define LOGD_IF(cond, s) \
    if (cond)            \
    LOGD(s)
#define LOGI_IF(cond, s) \
    if (cond)            \
    LOGI(s)
#define LOGW_IF(cond, s) \
    if (cond)            \
    LOGW(s) // ✅ 新增
#define LOGE_IF(cond, s) \
    if (cond)            \
    LOGE(s)
