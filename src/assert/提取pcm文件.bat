@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem 用途：从测试视频中提取 16 位小端序的原始 PCM 音频数据。
rem 输入：当前目录下的 v1080.mp4；输出：当前目录下的 out.pcm。
rem PCM 文件不包含采样率、声道数等元数据，播放时需参考源文件参数。
rem 如果 out.pcm 已存在，FFmpeg 会询问是否覆盖。

set "INPUT_FILE=v1080.mp4"
set "OUTPUT_FILE=out.pcm"

where ffmpeg >nul 2>&1 || (
    echo [提取PCM] 未找到 ffmpeg，请确认已安装并加入 PATH。
    pause
    exit /b 1
)
if not exist "%INPUT_FILE%" (
    echo [提取PCM] 未找到输入文件：%CD%\%INPUT_FILE%
    pause
    exit /b 1
)

echo ============================================================
echo [提取PCM] 输入文件：%CD%\%INPUT_FILE%
echo [提取PCM] 输出文件：%CD%\%OUTPUT_FILE%
echo [提取PCM] 输出格式：16 位小端序 PCM（s16le）
echo ============================================================
ffmpeg -i v1080.mp4 -f s16le out.pcm
if errorlevel 1 (
    echo [提取PCM] 提取失败，退出码：%ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo [提取PCM] 提取完成。
pause
exit /b 0
