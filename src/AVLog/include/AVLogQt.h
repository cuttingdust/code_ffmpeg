#pragma once

/// 将 qDebug/qInfo/qWarning 等 Qt 日志转发到 AVLog/MLog（需链接 Qt6::Core）
auto avLogInstallQtMessageHandler() -> void;

/// 恢复 Qt 默认控制台输出
auto avLogUninstallQtMessageHandler() -> void;
