@echo off
			echo ==========================================
			echo Deploying Qt libraries for ffmpeg_opengl...
			echo ==========================================
			echo.
			echo Target executable: F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/ffmpeg_opengl.exe
			echo.
			"E:/1Code/thirdparty/Qt6/bin/windeployqt.exe" "F:/thirty/MyFile/code_ffmpeg/src/../out/bin.x64/ffmpeg_opengl.exe" --openglwidgets --multimedia
			echo.
			if %errorlevel% equ 0 (
				echo [SUCCESS] Qt DLLs deployed successfully!
			) else (
				echo [ERROR] Deployment failed with error code %errorlevel%
			)
			echo.
			pause
		