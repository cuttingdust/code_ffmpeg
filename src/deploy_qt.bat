@echo off
echo ==========================================
echo Deploying Qt libraries for OpenGL apps...
echo Output directory: F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64
echo ==========================================
echo.
echo Deploying XCodecRtsp.exe...
"E:/1Code/thirdparty/Qt6/bin/windeployqt.exe" "F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/XCodecRtsp.exe" --openglwidgets --multimedia
if %errorlevel% neq 0 (
    echo [ERROR] XCodecRtsp deployment failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)
echo.
echo Deploying XView.exe...
"E:/1Code/thirdparty/Qt6/bin/windeployqt.exe" "F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/XView.exe" --openglwidgets --multimedia
if %errorlevel% neq 0 (
    echo [ERROR] XView deployment failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)
echo.
echo Deploying ffmpeg_opengl.exe...
"E:/1Code/thirdparty/Qt6/bin/windeployqt.exe" "F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/ffmpeg_opengl.exe" --openglwidgets --multimedia
if %errorlevel% neq 0 (
    echo [ERROR] ffmpeg_opengl deployment failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)
echo.
echo Deploying XPlay.exe...
"E:/1Code/thirdparty/Qt6/bin/windeployqt.exe" "F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/XPlay.exe" --openglwidgets --multimedia
if %errorlevel% neq 0 (
    echo [ERROR] XPlay deployment failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)
echo.
echo [SUCCESS] Qt DLLs deployed successfully!
echo.
pause
