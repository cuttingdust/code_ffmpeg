@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem 用途：将测试视频缩放为 600x300，并导出原始 YUV 视频数据。
rem 输入：当前目录下的 v1080.mp4；输出：当前目录下的 2.yuv。
rem 如需固定像素格式，建议在 ffmpeg 参数中显式增加 -pix_fmt。
rem 当前脚本保留 FFmpeg 自动选择的像素格式，以维持原有测试行为。

set "INPUT_FILE=v1080.mp4"
set "OUTPUT_FILE=2.yuv"

where ffmpeg >nul 2>&1 || (
    echo [生成YUV] 未找到 ffmpeg，请确认已安装并加入 PATH。
    pause
    exit /b 1
)
if not exist "%INPUT_FILE%" (
    echo [生成YUV] 未找到输入文件：%CD%\%INPUT_FILE%
    pause
    exit /b 1
)

echo ============================================================
echo [生成YUV] 输入文件：%CD%\%INPUT_FILE%
echo [生成YUV] 输出文件：%CD%\%OUTPUT_FILE%
echo [生成YUV] 输出尺寸：600x300
echo ============================================================
ffmpeg -i v1080.mp4 -s 600x300  2.yuv
if errorlevel 1 (
    echo [生成YUV] 转换失败，退出码：%ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo [生成YUV] 转换完成。
pause
exit /b 0
