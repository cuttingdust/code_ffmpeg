#include "AVLogQt.h"

#include "AVLog.h"

#include <QtCore/QString>
#include <cstring>

namespace
{

QtMessageHandler g_prev_qt_handler = nullptr;
bool             g_qt_handler_installed = false;
thread_local bool g_in_qt_message_handler = false;

class QtMessageHandlerGuard
{
public:
    QtMessageHandlerGuard()
    {
        g_in_qt_message_handler = true;
    }

    ~QtMessageHandlerGuard()
    {
        g_in_qt_message_handler = false;
    }
};

auto shouldSkipQtMessage(const QMessageLogContext& context, const QString& msg) -> bool
{
    /// 样式表解析噪声（已在 ui 中修复，保留过滤以防第三方控件）
    if (msg.contains(QStringLiteral("QCssParser")))
    {
        return true;
    }

    // Qt 内部 warning 可能发生在控件/OpenGL 析构阶段；此时同步转入 spdlog 容易踩到退出期生命周期。
    if (!context.file || *context.file == '\0')
    {
        return true;
    }
    if (context.category && std::strncmp(context.category, "qt.", 3) == 0)
    {
        return true;
    }

    return false;
}

void avQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (g_in_qt_message_handler || shouldSkipQtMessage(context, msg))
    {
        return;
    }

    QtMessageHandlerGuard guard;
    const QByteArray utf8 = msg.toUtf8();
    std::string          text = utf8.constData();

    if (context.category && *context.category != '\0')
    {
        text = std::string("[") + context.category + "] " + text;
    }

    const std::string file = context.file;
    const int   line = context.line > 0 ? context.line : 0;

    switch (type)
    {
        case QtDebugMsg:
            avLogWrite(XLOG_LEVEL_DEBUG, file.c_str(), line, text);
            break;
        case QtInfoMsg:
            avLogWrite(XLOG_LEVEL_INFO, file.c_str(), line, text);
            break;
        case QtWarningMsg:
            avLogWrite(XLOG_LEVEL_WARN, file.c_str(), line, text);
            break;
        case QtCriticalMsg:
            avLogWrite(XLOG_LEVEL_ERROR, file.c_str(), line, text);
            break;
        case QtFatalMsg:
            avLogWrite(XLOG_LEVEL_FATAL, file.c_str(), line, text);
            abort();
            break;
    }
}

} // namespace

void avLogInstallQtMessageHandler()
{
    if (g_qt_handler_installed)
    {
        return;
    }

    g_prev_qt_handler = qInstallMessageHandler(avQtMessageHandler);
    g_qt_handler_installed = true;
}

void avLogUninstallQtMessageHandler()
{
    if (!g_qt_handler_installed)
    {
        return;
    }

    qInstallMessageHandler(g_prev_qt_handler);
    g_prev_qt_handler = nullptr;
    g_qt_handler_installed = false;
}
