@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem 用途：将测试视频缩放为 200x180，并转换为 YUV420P 原始视频数据。
rem 输入：当前目录下的 1080.mp4；输出：当前目录下的 output.yuv。
rem 原始 YUV 文件不包含宽高和像素格式信息，播放时需要手动指定这些参数。
rem 如果 output.yuv 已存在，FFmpeg 会询问是否覆盖；脚本不会静默删除旧文件。

REM 旧示例：ffmpeg -i 400_300_25.mp4 400_300_25.yuv
set "INPUT_FILE=1080.mp4"
set "OUTPUT_FILE=output.yuv"

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
echo [生成YUV] 输出参数：200x180，yuv420p
echo ============================================================
ffmpeg -i 1080.mp4 -s 200x180 -pix_fmt yuv420p output.yuv
if errorlevel 1 (
    echo [生成YUV] 转换失败，退出码：%ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo [生成YUV] 转换完成。
pause
exit /b 0
