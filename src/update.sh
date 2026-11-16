#!/usr/bin/env bash
# 重新配置 src/build 中的 CMake 工程，不执行项目编译。
# 用法：bash src/update.sh [CMake 配置参数...]
# 示例：bash src/update.sh -G Xcode
#       bash src/update.sh -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos
# 不传 -G 时沿用已有缓存或 CMake 默认生成器；更换生成器前先清理 build。
# 前置条件：CMake、编译工具链、VCPKG_ROOT 以及项目需要的 Qt 等依赖。
# 本脚本不更新基线、不显式调用 vcpkg install；但 vcpkg 工具链可能在
# CMake 配置过程中自动恢复缺失依赖。也可提前运行 vcpkg_install.sh。
# macOS/Linux 不同步 Visual Studio 专用的 .sln.DotSettings 配置。
set -euo pipefail
trap 'code=$?; if (( code != 0 )); then printf "[update] 执行失败，退出码：%s\n" "$code" >&2; fi' EXIT

# 固定目录，不依赖调用者的工作目录；双引号保护含空格的路径。
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
BUILD_DIR="$SCRIPT_DIR/build"
command -v cmake >/dev/null 2>&1 || { echo '[update] 请先安装 CMake。' >&2; exit 1; }

# 禁止覆盖固定的源码/构建目录；其余选项以原始参数数组传给 CMake。
# 这样 clean/open/update 始终操作同一个构建目录。
for argument in "$@"; do
    case "$argument" in
        -S*|-B*|--build|--open|--install|--workflow|--preset*|-P*|-E|--)
            printf '[update] 不支持覆盖执行模式或目录的参数：%s\n' "$argument" >&2
            exit 2 ;;
    esac
done
printf '[update] 源码目录：%s\n[update] 构建目录：%s\n' "$SCRIPT_DIR" "$BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" "$@"
echo '[update] CMake 工程更新完成。'
