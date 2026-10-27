@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem 用途：完整重建 src\build 以及其中的 vcpkg 清单模式依赖目录。
rem 注意：会删除 src\build 下生成的工程、目标文件和已安装依赖。
rem 不会删除源代码，也不会修改 VCPKG_ROOT 指向的公共 vcpkg 仓库。
rem 删除目标固定为本脚本所在目录下的 build，避免误删其他目录。

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"

echo ============================================================
echo [clean] 准备重新创建构建目录
echo [clean] 即将删除：%BUILD_DIR%
echo [clean] 源代码和 VCPKG_ROOT 不会被删除
echo ============================================================
if exist "%BUILD_DIR%\" (
    echo [clean] 正在删除旧构建目录...
    rmdir /s /q "%BUILD_DIR%"
    if exist "%BUILD_DIR%\" (
        echo [clean] 删除失败，目录可能正被 Visual Studio 或其他程序占用。
        pause
        exit /b 1
    )
    echo [clean] 旧构建目录已删除。
) else (
    echo [clean] 构建目录不存在，无需删除。
)

pushd "%SCRIPT_DIR%" || exit /b 1
echo [clean] 正在重新配置 CMake 工程...
echo [clean] 执行命令：cmake -S . -B build
cmake -S . -B build
if errorlevel 1 goto :error

echo [clean] 正在同步 ReSharper 团队配置...
call "%SCRIPT_DIR%sync_dotsettings.bat" build --no-pause
if errorlevel 1 goto :error
echo [clean] 构建目录重新创建完成。
pause
popd
endlocal
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [clean] 执行失败，退出码：%EXIT_CODE%
pause
popd
endlocal & exit /b %EXIT_CODE%
