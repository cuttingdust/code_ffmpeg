@echo off
if defined UPDATE_BAT_UTF8_READY goto :utf8_ready
chcp 65001 >nul
setlocal
set "UPDATE_BAT_UTF8_READY=1"
cmd.exe /d /c ""%~f0" %*"
set "BOOTSTRAP_EXIT_CODE=%ERRORLEVEL%"
endlocal & exit /b %BOOTSTRAP_EXIT_CODE%

:utf8_ready
setlocal EnableExtensions

rem 用途：在 src\build 中重新生成 Visual Studio/CMake 工程。
rem 本脚本不会更新 vcpkg 基线，也不会主动安装依赖。
rem 修改 vcpkg.json 或基线后，请先运行 vcpkg_install.bat。

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
pushd "%SCRIPT_DIR%" || exit /b 1

echo ============================================================
echo [update] 开始更新 CMake 工程
echo [update] 源码目录：%SCRIPT_DIR%
echo [update] 构建目录：%BUILD_DIR%
echo [update] 执行命令：cmake -S . -B build
echo ============================================================
cmake -S . -B build
if errorlevel 1 goto :error

rem 将团队共享的 ReSharper 配置复制到生成的解决方案旁边。
echo [update] 正在同步 ReSharper 团队配置...
call "%SCRIPT_DIR%sync_dotsettings.bat" build --no-pause
if errorlevel 1 goto :error

echo [update] CMake 工程更新完成。
pause
popd
endlocal
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [update] 执行失败，退出码：%EXIT_CODE%
pause
popd
endlocal & exit /b %EXIT_CODE%
