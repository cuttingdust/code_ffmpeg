#!/usr/bin/env bash
# 循环读取本地视频，按原始速度推送给 MediaMTX；先启动 mock_rtsp_mediamtx.sh。
# 用法：bash src/assert/mock_rtsp_ffmpeg.sh [视频路径] [RTSP 地址]
# 默认选用项目已有的 400_300_25.mp4，默认推流到 rtsp://127.0.0.1:8554/main。
# 相对视频路径基于本脚本目录；绝对路径也可用。引号保护含空格的路径。
# -c copy 不重新编码，源视频编码需被 RTSP 接受；-rtsp_transport tcp 使用 TCP 推流。
# exec 将前台进程替换成 FFmpeg，Ctrl+C 可直接终止，不残留后台循环。
# 优先使用同目录独立程序；仅在缺失时回退到 PATH 中的 ffmpeg。
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
if (( $# > 2 )); then
    echo '用法：mock_rtsp_ffmpeg.sh [视频路径] [RTSP 地址]' >&2
    exit 2
fi
INPUT_FILE=${1:-400_300_25.mp4}
RTSP_URL=${2:-rtsp://127.0.0.1:8554/main}
cd -- "$SCRIPT_DIR"
[[ -f "$INPUT_FILE" ]] || { printf '[ffmpeg] 视频不存在：%s\n' "$INPUT_FILE" >&2; exit 1; }
if [[ -x "$SCRIPT_DIR/ffmpeg" ]]; then
    FFMPEG="$SCRIPT_DIR/ffmpeg"
else
    FFMPEG=$(command -v ffmpeg) || { echo '[ffmpeg] 缺少本地程序，PATH 中也未找到 ffmpeg。' >&2; exit 1; }
fi
printf '[ffmpeg] 输入：%s\n[ffmpeg] 推流地址：%s\n' "$INPUT_FILE" "$RTSP_URL"
exec "$FFMPEG" -re -stream_loop -1 -i "$INPUT_FILE" -c copy -rtsp_transport tcp -f rtsp "$RTSP_URL"
