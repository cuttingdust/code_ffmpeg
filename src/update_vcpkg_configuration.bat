@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem 用途：更新 vcpkg-configuration.json 中的注册表基线。
rem 实际处理逻辑位于同名 PowerShell 脚本中，并会保留 mlog 自定义注册表。
rem
rem 常用方式：
rem   update_vcpkg_configuration.bat
rem       使用本机 VCPKG_ROOT 当前提交作为内置注册表基线。
rem   update_vcpkg_configuration.bat -PullVcpkg
rem       先快进更新 VCPKG_ROOT，再使用更新后的 HEAD。
rem   update_vcpkg_configuration.bat -DefaultFromRemote
rem       只查询 microsoft/vcpkg 远端 HEAD，不改动本地仓库。
rem   update_vcpkg_configuration.bat --no-pause
rem       被其他脚本调用时，结束后不暂停。

set "SCRIPT_DIR=%~dp0"
set "NO_PAUSE="
set "POWERSHELL_ARGS="

:parse_args
if "%~1"=="" goto :run
if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
) else if /I "%~1"=="-PullVcpkg" (
    set "POWERSHELL_ARGS=%POWERSHELL_ARGS% -PullVcpkg"
) else if /I "%~1"=="-DefaultFromRemote" (
    set "POWERSHELL_ARGS=%POWERSHELL_ARGS% -DefaultFromRemote"
) else (
    echo [vcpkg-config] 不支持的参数：%~1
    endlocal & exit /b 2
)
shift
goto :parse_args

:run
echo ============================================================
echo [vcpkg-config] 准备更新 vcpkg 注册表基线
echo [vcpkg-config] 配置文件：%SCRIPT_DIR%vcpkg-configuration.json
if defined POWERSHELL_ARGS (
    echo [vcpkg-config] PowerShell 参数：%POWERSHELL_ARGS%
) else (
    echo [vcpkg-config] PowerShell 参数：无，使用本地 VCPKG_ROOT 当前提交
)
echo ============================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%update_vcpkg_configuration.ps1" %POWERSHELL_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" (
    echo [vcpkg-config] 注册表基线更新完成。
) else (
    echo [vcpkg-config] 更新失败，退出码：%EXIT_CODE%
)
if not defined NO_PAUSE pause
endlocal & exit /b %EXIT_CODE%
