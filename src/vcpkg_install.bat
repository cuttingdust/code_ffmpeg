@echo off
chcp 65001 > nul

echo ============================================================
echo  Updating vcpkg-configuration.json
echo ============================================================
call "%~dp0update_vcpkg_configuration.bat" --no-pause
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to update vcpkg-configuration.json.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Running vcpkg install
echo ============================================================
vcpkg install
if errorlevel 1 (
    echo.
    echo [ERROR] vcpkg install failed.
    pause
    exit /b 1
)

powershell -NoProfile -Command "Start-Sleep -Seconds 5"
