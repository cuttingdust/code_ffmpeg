@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem 用途：管理 Git 全局 safe.directory 配置，解决仓库所有者不一致时的安全提示。
rem 默认操作对象为运行脚本时所在的当前目录，而不是脚本自身所在目录。
rem
rem 使用方式：
rem   git.directory.bat add          将当前目录加入 Git 安全目录。
rem   git.directory.bat del          从 Git 安全目录中移除当前目录。
rem   git.directory.bat list         列出全部 Git 安全目录。
rem   git.directory.bat clean --force  清空全部全局安全目录，必须显式确认。

if "%~1"=="" goto :usage

for %%I in (.) do set "GIT_PATH=%%~fI"
set "GIT_PATH=!GIT_PATH:\=/!"

echo [git.directory] 请求操作：%~1
echo [git.directory] 当前目录：!GIT_PATH!

if /I "%~1"=="add" goto :add
if /I "%~1"=="del" goto :del
if /I "%~1"=="list" goto :list
if /I "%~1"=="clean" goto :clean
goto :usage

:add
git config --global --add safe.directory "!GIT_PATH!"
if errorlevel 1 goto :error
echo [git.directory] 已添加安全目录：!GIT_PATH!
pause
exit /b 0

:del
git config --global --unset-all safe.directory "!GIT_PATH!" 2>nul
if errorlevel 1 (
    echo [git.directory] 当前目录不在安全目录列表中：!GIT_PATH!
    pause
    exit /b 0
)
echo [git.directory] 已移除安全目录：!GIT_PATH!
pause
exit /b 0

:list
echo [git.directory] 当前全局安全目录：
git config --global --get-all safe.directory
if errorlevel 1 echo [git.directory] 尚未配置安全目录。
pause
exit /b 0

:clean
if /I not "%~2"=="--force" (
    echo [git.directory] clean 会清空全部全局安全目录。
    echo [git.directory] 如确认执行，请使用：git.directory.bat clean --force
    pause
    exit /b 2
)
git config --global --unset-all safe.directory 2>nul
echo [git.directory] 已清空全部全局安全目录。
pause
exit /b 0

:usage
echo [git.directory] 用于管理 Git 全局 safe.directory，不会修改仓库内容。
echo 用法：git.directory.bat add^|del^|list^|clean --force
pause
exit /b 2

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [git.directory] Git 配置操作失败，退出码：%EXIT_CODE%
pause
exit /b %EXIT_CODE%
