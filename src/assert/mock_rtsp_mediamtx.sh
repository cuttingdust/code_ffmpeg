#!/usr/bin/env bash
# 启动同目录 MediaMTX，读取现有 mediamtx.yml；不依赖 PATH 或 Homebrew。
# 用法：bash src/assert/mock_rtsp_mediamtx.sh
# 保持前台运行，Ctrl+C 结束；不按进程名称批量杀死其他 MediaMTX 实例。
# 如提示端口占用，先检查现有实例；配置中的路径需要先有发布者推流才能拉流。
# 当前附带的二进制为 macOS arm64；其他平台需要替换成匹配的原生版本。
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
[[ -x "$SCRIPT_DIR/mediamtx" ]] || { echo '[mediamtx] 缺少同目录可执行文件 mediamtx。' >&2; exit 1; }
[[ -f "$SCRIPT_DIR/mediamtx.yml" ]] || { echo '[mediamtx] 缺少 mediamtx.yml。' >&2; exit 1; }
cd -- "$SCRIPT_DIR"
echo '[mediamtx] 启动 RTSP 服务，按 Ctrl+C 停止。'
exec "$SCRIPT_DIR/mediamtx" "$SCRIPT_DIR/mediamtx.yml"
