@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem Sync ReSharper team-shared *.sln.DotSettings from repo root into build tree.
rem Naming: MyRepo.sln -> repo root/MyRepo.sln.DotSettings (same as CMake project parent folder name).
rem Usage: sync_dotsettings.bat [build_dir_relative_to_src]   default: build

set "SRC_DIR=%~dp0"
if "%~1"=="" (set "BUILD_DIR=%SRC_DIR%build") else (set "BUILD_DIR=%SRC_DIR%%~1")
for %%I in ("%SRC_DIR%..") do set "REPO_ROOT=%%~fI"

if not exist "%BUILD_DIR%\" exit /b 0

rem Match each *.sln under build/ (top-level and nested subprojects)
for /r "%BUILD_DIR%" %%S in (*.sln) do (
    if exist "%REPO_ROOT%\%%~nxS.DotSettings" (
        copy /Y "%REPO_ROOT%\%%~nxS.DotSettings" "%%~dpS" >nul
    )
)

rem Fallback before first cmake: copy all team settings to build root
if not exist "%BUILD_DIR%\*.sln" (
    for %%D in ("%REPO_ROOT%\*.sln.DotSettings") do (
        copy /Y "%%~fD" "%BUILD_DIR%\" >nul
    )
)

endlocal
exit /b 0
