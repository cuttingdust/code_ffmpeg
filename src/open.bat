@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem 用途：使用 CMake 配置的 IDE 打开已生成的解决方案。
rem 如果 src\build 尚未完成配置，会先自动执行一次 CMake 配置。

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" || exit /b 1

echo ============================================================
echo [open] 准备打开 CMake 生成的解决方案
echo [open] 工程目录：%SCRIPT_DIR%
echo [open] 构建目录：%SCRIPT_DIR%build
echo ============================================================

if not exist "build\CMakeCache.txt" (
    rem CMakeCache.txt 是构建目录已完成配置的标志；缺失时先生成工程。
    echo [open] 未找到 build\CMakeCache.txt，先执行 CMake 配置...
    cmake -S . -B build
    if errorlevel 1 goto :error
    echo [open] CMake 配置完成。
)

echo [open] 正在调用 cmake --open build...
cmake --open build
if errorlevel 1 goto :error
echo [open] 已向系统发送打开解决方案的请求。

pause
popd
endlocal
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [open] 执行失败，退出码：%EXIT_CODE%
pause
popd
endlocal & exit /b %EXIT_CODE%
