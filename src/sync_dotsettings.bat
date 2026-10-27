@echo off
if defined SYNC_DOTSETTINGS_UTF8_READY goto :utf8_ready
chcp 65001 >nul
set "SYNC_DOTSETTINGS_UTF8_READY=1"
cmd.exe /d /c ""%~f0" %*"
exit /b %errorlevel%

:utf8_ready
setlocal EnableExtensions EnableDelayedExpansion

rem 用途：将仓库根目录中的 ReSharper 团队共享配置复制到构建目录。
rem 命名规则：MyRepo.sln 对应仓库根目录下的 MyRepo.sln.DotSettings。
rem 使用方式：sync_dotsettings.bat [相对于 src 的构建目录] [--no-pause]。
rem 构建目录默认为 build；--no-pause 用于被 update/clean 脚本调用。

set "SRC_DIR=%~dp0"
set "NO_PAUSE="
if "%~1"=="" (set "BUILD_DIR=%SRC_DIR%build") else (set "BUILD_DIR=%SRC_DIR%%~1")
if /I "%~1"=="--no-pause" (
    set "BUILD_DIR=%SRC_DIR%build"
    set "NO_PAUSE=1"
)
if /I "%~2"=="--no-pause" set "NO_PAUSE=1"
for %%I in ("%SRC_DIR%..") do set "REPO_ROOT=%%~fI"
set /a COPY_COUNT=0

echo [dotsettings] 仓库目录：%REPO_ROOT%
echo [dotsettings] 构建目录：%BUILD_DIR%

if not exist "%BUILD_DIR%\" (
    rem 构建目录不存在时不视为错误，调用者可能尚未执行 CMake。
    echo [dotsettings] 构建目录不存在，跳过同步。
    if not defined NO_PAUSE pause
    exit /b 0
)

rem 匹配 build 下所有解决方案，包括顶层工程和嵌套子工程。
for /r "%BUILD_DIR%" %%S in (*.sln) do (
    if exist "%REPO_ROOT%\%%~nxS.DotSettings" (
        copy /Y "%REPO_ROOT%\%%~nxS.DotSettings" "%%~dpS" >nul
        if not errorlevel 1 (
            set /a COPY_COUNT+=1
            echo [dotsettings] 已复制：%%~nxS.DotSettings -^> %%~dpS
        )
    )
)

rem 首次运行 CMake、尚无解决方案文件时，将全部团队配置复制到 build 根目录。
if not exist "%BUILD_DIR%\*.sln" if exist "%REPO_ROOT%\*.sln.DotSettings" (
    for %%D in ("%REPO_ROOT%\*.sln.DotSettings") do (
        copy /Y "%%~fD" "%BUILD_DIR%\" >nul
        if not errorlevel 1 (
            set /a COPY_COUNT+=1
            echo [dotsettings] 已复制到构建根目录：%%~nxD
        )
    )
)

echo [dotsettings] 同步完成，共复制 !COPY_COUNT! 个配置文件。
if not defined NO_PAUSE pause
endlocal
exit /b 0
