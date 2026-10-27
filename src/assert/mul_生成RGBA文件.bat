@echo off
chcp 65001 >nul
cd /d "%~dp0"

rem 用途：生成 800x400、RGBA 像素格式的原始测试画面。
rem 输入：当前目录下的 v1080.mp4；输出：当前目录下的 1.rgb。
rem RGBA 每像素占 4 字节，可按“宽 x 高 x 4”计算单帧数据大小。

set "INPUT_FILE=v1080.mp4"
set "OUTPUT_FILE=1.rgb"

where ffmpeg >nul 2>&1 || (
    echo [生成RGBA] 未找到 ffmpeg，请确认已安装并加入 PATH。
    pause
    exit /b 1
)
if not exist "%INPUT_FILE%" (
    echo [生成RGBA] 未找到输入文件：%CD%\%INPUT_FILE%
    pause
    exit /b 1
)

echo ============================================================
echo [生成RGBA] 输入文件：%CD%\%INPUT_FILE%
echo [生成RGBA] 输出文件：%CD%\%OUTPUT_FILE%
echo [生成RGBA] 输出参数：800x400，rgba
echo ============================================================
ffmpeg -i v1080.mp4 -s 800x400  -pix_fmt rgba 1.rgb
if errorlevel 1 (
    echo [生成RGBA] 转换失败，退出码：%ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo [生成RGBA] 转换完成。
pause
exit /b 0
