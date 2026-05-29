@echo off
ffmpeg -re -stream_loop -1 -i output.mp4 -c copy -f rtsp rtsp://127.0.0.1:8554/live/stream