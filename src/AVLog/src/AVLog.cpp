#include "AVLog.h"

#include <MLog/MLog.h>

#include <spdlog/spdlog.h>

namespace
{
    auto toSpdLevel(XLogLevel level) -> spdlog::level::level_enum
    {
        switch (level)
        {
            case XLOG_LEVEL_DEBUG:
                return spdlog::level::debug;
            case XLOG_LEVEL_INFO:
                return spdlog::level::info;
            case XLOG_LEVEL_WARN:
                return spdlog::level::warn;
            case XLOG_LEVEL_ERROR:
                return spdlog::level::err;
            case XLOG_LEVEL_FATAL:
                return spdlog::level::critical;
            default:
                return spdlog::level::info;
        }
    }
} // namespace

void avLogInit()
{
    (void)MLog::Instance()->ResetLogger(LogTarget::CONSOLE_FILE_MSVC);
}

void avLogSetMinLevel(XLogLevel level)
{
    if (auto logger = MLog::Instance()->GetLogger())
    {
        logger->set_level(toSpdLevel(level));
    }
}

void avLogWrite(XLogLevel level, const char* file, int line, const std::string& message)
{
    auto logger = MLog::Instance()->GetLogger();
    if (!logger)
    {
        return;
    }

    logger->log(spdlog::source_loc{ file, line, SPDLOG_FUNCTION }, toSpdLevel(level), "[{}:{}] {}", file, line,
                message);
}
