@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem 用途：为当前仓库或全部仓库配置 Git HTTP/HTTPS 代理。
rem 默认代理地址为 http://127.0.0.1:7890，如端口变化请修改 PROXY_URL。
rem
rem 使用方式：
rem   git.proxy.bat on              开启当前仓库代理。
rem   git.proxy.bat off             关闭当前仓库代理。
rem   git.proxy.bat status          查看当前仓库及全局代理。
rem   git.proxy.bat global on       开启全局代理。
rem   git.proxy.bat global off      关闭全局代理。

set "PROXY_URL=http://127.0.0.1:7890"

echo [git.proxy] 请求操作：%*
echo [git.proxy] 代理地址：%PROXY_URL%

if /I "%~1"=="on" goto :local_on
if /I "%~1"=="off" goto :local_off
if /I "%~1"=="status" goto :status
if /I "%~1"=="global" goto :global
goto :usage

:local_on
git config --local http.proxy "%PROXY_URL%" || goto :error
git config --local https.proxy "%PROXY_URL%" || goto :error
echo [git.proxy] 已开启当前仓库代理：%PROXY_URL%
exit /b 0

:local_off
git config --local --unset-all http.proxy 2>nul
git config --local --unset-all https.proxy 2>nul
echo [git.proxy] 已关闭当前仓库代理。
exit /b 0

:global
if /I "%~2"=="on" goto :global_on
if /I "%~2"=="off" goto :global_off
goto :usage

:global_on
git config --global http.proxy "%PROXY_URL%" || goto :error
git config --global https.proxy "%PROXY_URL%" || goto :error
echo [git.proxy] 已开启全局代理：%PROXY_URL%
exit /b 0

:global_off
git config --global --unset-all http.proxy 2>nul
git config --global --unset-all https.proxy 2>nul
echo [git.proxy] 已关闭全局代理。
exit /b 0

:status
echo [git.proxy] 当前仓库 HTTP 代理：
git config --local --get http.proxy 2>nul || echo   未配置
echo [git.proxy] 当前仓库 HTTPS 代理：
git config --local --get https.proxy 2>nul || echo   未配置
echo [git.proxy] 全局 HTTP 代理：
git config --global --get http.proxy 2>nul || echo   未配置
echo [git.proxy] 全局 HTTPS 代理：
git config --global --get https.proxy 2>nul || echo   未配置
exit /b 0

:usage
echo [git.proxy] on/off 只影响当前仓库，global 操作影响当前用户的全部仓库。
echo 用法：git.proxy.bat on^|off^|status^|global on^|global off
exit /b 2

:error
set "EXIT_CODE=%ERRORLEVEL%"
echo [git.proxy] Git 配置操作失败，退出码：%EXIT_CODE%
exit /b %EXIT_CODE%
