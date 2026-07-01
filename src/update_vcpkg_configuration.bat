@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "NO_PAUSE="

if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
    shift /1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%update_vcpkg_configuration.ps1" %*
if not defined NO_PAUSE pause
exit /b %ERRORLEVEL%
