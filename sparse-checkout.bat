@echo off
REM Windows 本地目录整理入口，克隆项目后运行一次即可。
REM 用法：sparse-checkout.bat           隐藏所有目录中的 .sh 文件。
REM       sparse-checkout.bat --disable 恢复显示全部已跟踪文件。
REM
REM Git 仍保存完整历史；只改变本地检出范围，不删除仓库文件、不提交或推送。
REM 配置保存在本地 Git 元数据中，后续 pull 沿用；新 clone 需再次执行。
REM 这会替换本地原有稀疏规则，需要保留的自定义规则请先自行保存。
REM 未提交修改可能使 Git 保留文件并提示；请先提交或妥善保存修改。
REM 未跟踪文件不受稀疏检出控制，本脚本不会强制清理磁盘文件。
setlocal EnableExtensions DisableDelayedExpansion
REM 临时切换编码用于中文提示，避免永久改变调用终端的代码页。
for /f "tokens=2 delims=:" %%C in ('chcp') do set "ORIGINAL_CODEPAGE=%%C"
chcp 65001 >nul
set "EXIT_CODE=0"
REM 使用脚本位置定位项目，不依赖启动时的工作目录。
git -C "%~dp0." rev-parse --show-toplevel >nul 2>&1
if errorlevel 1 (
    echo 请确认 Git 已安装，且脚本位于 Git 仓库内。
    set "EXIT_CODE=1"
    goto :finish
)
if not "%~2"=="" goto :usage
if /I "%~1"=="--disable" goto :disable
if not "%~1"=="" goto :usage

REM no-cone 支持文件模式，先包含全部，再排除任意目录中的 .sh 后缀。
REM 双引号保护规则；DisableDelayedExpansion 防止感叹号被 CMD 展开。
REM 字符组兼容 .sh、.SH 以及混合大小写；Shell 入口自身也会被隐藏。
git -C "%~dp0." sparse-checkout set --no-cone "/*" "!*.[sS][hH]"
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" echo 已隐藏已跟踪的 .sh 文件，后续 pull 沿用此设置。
goto :finish

:disable
git -C "%~dp0." sparse-checkout disable
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" echo 已恢复显示全部已跟踪文件。
goto :finish

:usage
echo 用法：sparse-checkout.bat [--disable]
set "EXIT_CODE=2"

:finish
REM 保存并透传退出码，使其他自动化能够识别失败；不等待按键。
if not "%EXIT_CODE%"=="0" echo 稀疏检出设置失败，退出码：%EXIT_CODE%
if defined ORIGINAL_CODEPAGE chcp %ORIGINAL_CODEPAGE% >nul
endlocal & exit /b %EXIT_CODE%
