chcp 65001
# 使用 8554 端口（默认）
vlc -vvv video.mp4 --sout '#rtp{sdp=rtsp://:8554/test}' --loop