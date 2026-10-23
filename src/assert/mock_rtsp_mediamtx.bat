@echo off
chcp 65001 >nul
cd /d "%~dp0"

REM 用途：启动当前目录下的 MediaMTX，作为本地 RTSP 测试服务器。
REM 启动前会结束残留的 mediamtx.exe，避免 8000、8554 等端口被旧进程占用。
REM 注意：taskkill 会结束系统中所有名为 mediamtx.exe 的进程。
REM MediaMTX 默认读取当前目录的 mediamtx.yml；关闭本窗口不一定会终止后台进程。

if not exist "mediamtx.exe" (
    echo [mock_rtsp_mediamtx] 未找到：%CD%\mediamtx.exe
    exit /b 1
)

echo ============================================================
echo [mock_rtsp_mediamtx] 工作目录：%CD%
echo [mock_rtsp_mediamtx] 配置文件：%CD%\mediamtx.yml
echo [mock_rtsp_mediamtx] RTSP 地址：rtsp://127.0.0.1:8554/main
echo ============================================================

REM 若上次窗口已关但 mediamtx 仍在后台，UDP 8000/8554 会被占用，新实例会 bind 失败
tasklist /FI "IMAGENAME eq mediamtx.exe" 2>nul | find /I "mediamtx.exe" >nul
if %ERRORLEVEL%==0 (
    echo [mock_rtsp_mediamtx] 发现旧进程，正在结束 mediamtx.exe...
    taskkill /IM mediamtx.exe /F >nul 2>&1
    timeout /t 1 /nobreak >nul
)

echo [mock_rtsp_mediamtx] 正在启动 mediamtx.exe...
mediamtx.exe
set "EXIT_CODE=%ERRORLEVEL%"
echo [mock_rtsp_mediamtx] MediaMTX 已结束，退出码：%EXIT_CODE%
pause
exit /b %EXIT_CODE%
