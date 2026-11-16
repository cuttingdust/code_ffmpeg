#!/usr/bin/env bash
# 打开 src/build 已生成的 IDE 工程；缺少缓存时先调用 update.sh 配置。
# 用法：bash src/open.sh
# macOS 首次生成 Xcode 工程：bash src/open.sh -G Xcode
# 带参数时会先重新配置，参数规则与 update.sh 相同。
# 已有 Ninja/Makefiles 缓存不能通过 -G 直接换成 Xcode；需先清理再配置。
# cmake --open 要求生成器支持 IDE 打开；Ninja/Unix Makefiles 不具备此能力。
# 若使用这类生成器，可直接用自己的编辑器打开源码目录。
set -euo pipefail
trap 'code=$?; if (( code != 0 )); then printf "[open] 执行失败，退出码：%s\n" "$code" >&2; fi' EXIT

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
BUILD_DIR="$SCRIPT_DIR/build"
command -v cmake >/dev/null 2>&1 || { echo '[open] 请先安装 CMake。' >&2; exit 1; }

# 与 Windows 入口一致，以缓存文件判断是否需要先配置；配置失败不继续打开。
# 传入参数时主动更新，让首次调用可选择 -G Xcode 或传入 Qt 查找路径。
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]] || (( $# > 0 )); then
    bash "$SCRIPT_DIR/update.sh" "$@"
fi
printf '[open] 正在打开工程：%s\n' "$BUILD_DIR"
if cmake --open "$BUILD_DIR"; then
    echo '[open] 已发送打开工程的请求。'
else
    code=$?
    echo '[open] 请检查构建目录的生成器是否支持 IDE 打开（例如 Xcode）。' >&2
    exit "$code"
fi
