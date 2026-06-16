@echo off
cd /d "%~dp0"

REM 若上次窗口已关但 mediamtx 仍在后台，UDP 8000/8554 会被占用，新实例会 bind 失败
tasklist /FI "IMAGENAME eq mediamtx.exe" 2>nul | find /I "mediamtx.exe" >nul
if %ERRORLEVEL%==0 (
    echo [mock_rtsp_mediamtx] stopping previous mediamtx.exe ...
    taskkill /IM mediamtx.exe /F >nul 2>&1
    timeout /t 1 /nobreak >nul
)

echo [mock_rtsp_mediamtx] starting mediamtx.exe ...
mediamtx.exe
pause