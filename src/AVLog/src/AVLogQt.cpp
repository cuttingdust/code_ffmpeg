#include "AVLogQt.h"

#include "AVLog.h"

#include <QtCore/QString>

namespace
{

QtMessageHandler g_prev_qt_handler = nullptr;

auto shouldSkipQtMessage(const QString& msg) -> bool
{
    /// 样式表解析噪声（已在 ui 中修复，保留过滤以防第三方控件）
    if (msg.contains(QStringLiteral("QCssParser")))
    {
        return true;
    }
    return false;
}

void avQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (shouldSkipQtMessage(msg))
    {
        return;
    }

    const QByteArray utf8 = msg.toUtf8();
    std::string          text = utf8.constData();

    if (context.category && *context.category != '\0')
    {
        text = std::string("[") + context.category + "] " + text;
    }

    const char* file = (context.file && *context.file != '\0') ? context.file : "Qt";
    const int   line = context.line > 0 ? context.line : 0;

    switch (type)
    {
        case QtDebugMsg:
            avLogWrite(XLOG_LEVEL_DEBUG, file, line, text);
            break;
        case QtInfoMsg:
            avLogWrite(XLOG_LEVEL_INFO, file, line, text);
            break;
        case QtWarningMsg:
            avLogWrite(XLOG_LEVEL_WARN, file, line, text);
            break;
        case QtCriticalMsg:
            avLogWrite(XLOG_LEVEL_ERROR, file, line, text);
            break;
        case QtFatalMsg:
            avLogWrite(XLOG_LEVEL_FATAL, file, line, text);
            abort();
            break;
    }
}

} // namespace

void avLogInstallQtMessageHandler()
{
    g_prev_qt_handler = qInstallMessageHandler(avQtMessageHandler);
}

void avLogUninstallQtMessageHandler()
{
    qInstallMessageHandler(g_prev_qt_handler);
    g_prev_qt_handler = nullptr;
}
