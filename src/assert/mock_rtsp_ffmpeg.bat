@echo off
REM 先运行 mock_rtsp_mediamtx.bat 启动 MediaMTX；推流路径须与 mediamtx.yml 中 paths 一致（main 允许 publisher）
ffmpeg -re -stream_loop -1 -i output.mp4 -c copy -f rtsp rtsp://127.0.0.1:8554/main