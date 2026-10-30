@echo off
if defined VCPKG_INSTALL_UTF8_READY goto :utf8_ready
chcp 65001 >nul
setlocal
set "VCPKG_INSTALL_UTF8_READY=1"
cmd.exe /d /c ""%~f0" %*"
set "BOOTSTRAP_EXIT_CODE=%ERRORLEVEL%"
endlocal & exit /b %BOOTSTRAP_EXIT_CODE%

:utf8_ready
setlocal EnableExtensions

REM 用途：根据 src\vcpkg.json 安装项目依赖。
REM 安装目录固定为 CMake 实际使用的 src\build\vcpkg_installed。
REM
REM 使用方式：
REM   vcpkg_install.bat                    安装当前基线锁定的依赖版本。
REM   vcpkg_install.bat --update-baseline  先更新注册表基线，再安装依赖。
REM
REM 更新基线可能导致大量库重新编译，也可能带来 API 兼容性变化。
REM 因此默认不自动更新基线，保证普通依赖恢复过程可重复。

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%.") do set "PROJECT_ROOT=%%~fI"
set "INSTALL_ROOT=%PROJECT_ROOT%\build\vcpkg_installed"
set "UPDATE_BASELINE="

if /I "%~1"=="--update-baseline" set "UPDATE_BASELINE=1"
if not "%~1"=="" if not defined UPDATE_BASELINE (
    echo [vcpkg] 不支持的参数：%~1
    echo [vcpkg] 用法：vcpkg_install.bat [--update-baseline]
    pause
    exit /b 2
)

if not defined VCPKG_ROOT (
    echo [vcpkg] 未设置环境变量 VCPKG_ROOT。
    echo [vcpkg] 请将其设置为 vcpkg 仓库目录，例如 F:\tools\vcpkg。
    pause
    exit /b 1
)

set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
if not exist "%VCPKG_EXE%" (
    echo [vcpkg] 未找到可执行文件：%VCPKG_EXE%
    pause
    exit /b 1
)

echo ============================================================
echo [vcpkg] 开始恢复项目依赖
echo [vcpkg] 清单文件：%PROJECT_ROOT%\vcpkg.json
echo [vcpkg] vcpkg 根目录：%VCPKG_ROOT%
echo [vcpkg] 安装目录：%INSTALL_ROOT%
echo [vcpkg] 目标平台：x64-windows
if defined UPDATE_BASELINE (
    echo [vcpkg] 基线策略：先更新基线，再安装
) else (
    echo [vcpkg] 基线策略：使用当前锁定基线
)
echo ============================================================

rem 重新引导 vcpkg.exe，确保它与当前 vcpkg 仓库中的脚本版本一致。
if exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
    echo [vcpkg] 正在同步 vcpkg 可执行工具与仓库脚本版本...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 goto :error
)

if defined UPDATE_BASELINE (
    echo [vcpkg] 正在更新 vcpkg 注册表基线...
    call "%SCRIPT_DIR%update_vcpkg_configuration.bat" --no-pause
    if errorlevel 1 goto :error
)

echo [vcpkg] 正在解析并安装依赖，此过程可能持续较长时间...

"%VCPKG_EXE%" install ^
    --x-manifest-root="%PROJECT_ROOT%" ^
    --x-install-root="%INSTALL_ROOT%" ^
    --triplet x64-windows
if errorlevel 1 goto :error

echo [vcpkg] 所有依赖均已安装完成。
pause
endlocal
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [vcpkg] 操作失败，退出码：%EXIT_CODE%
pause
endlocal & exit /b %EXIT_CODE%
