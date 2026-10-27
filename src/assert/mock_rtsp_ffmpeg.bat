@echo off
chcp 65001 >nul
cd /d "%~dp0"

REM 用途：循环读取 output.mp4，并将其作为 RTSP 测试流推送到本机 MediaMTX。
REM 请先运行 mock_rtsp_mediamtx.bat；推流路径须与 mediamtx.yml 中 paths 一致。
REM 当前使用 -c copy，不重新编码，因此源文件编码必须能被 RTSP 服务和播放器接受。
REM -re 按源文件原始速度读取，-stream_loop -1 表示无限循环，Ctrl+C 可停止推流。

set "INPUT_FILE=output.mp4"
set "RTSP_URL=rtsp://127.0.0.1:8554/main"

where ffmpeg >nul 2>&1 || (
    echo [mock_rtsp_ffmpeg] 未找到 ffmpeg，请确认已安装并加入 PATH。
    pause
    exit /b 1
)
if not exist "%INPUT_FILE%" (
    echo [mock_rtsp_ffmpeg] 未找到输入文件：%CD%\%INPUT_FILE%
    pause
    exit /b 1
)

echo ============================================================
echo [mock_rtsp_ffmpeg] 输入文件：%CD%\%INPUT_FILE%
echo [mock_rtsp_ffmpeg] 推流地址：%RTSP_URL%
echo [mock_rtsp_ffmpeg] 按 Ctrl+C 可停止推流
echo ============================================================
ffmpeg -re -stream_loop -1 -i output.mp4 -c copy -f rtsp rtsp://127.0.0.1:8554/main
set "EXIT_CODE=%ERRORLEVEL%"
echo [mock_rtsp_ffmpeg] FFmpeg 已结束，退出码：%EXIT_CODE%
pause
exit /b %EXIT_CODE%
