#!/usr/bin/env bash
# 根据同目录的 vcpkg.json 安装依赖，不依赖调用时的工作目录。
# 用法：./vcpkg_install.sh [--update-baseline]
# 默认使用锁定基线；更新基线需要 Python 3 和 Git，可能触发大量重新编译。
# 前置条件：VCPKG_ROOT 指向已经 bootstrap 的 vcpkg 仓库。
# 例如：export VCPKG_ROOT="$HOME/tools/vcpkg"。只安装依赖，不编译项目。
# -e 使失败立即停止，-u 防止引用未定义变量；pipefail 让管道中的失败也能被发现。
set -euo pipefail

trap 'code=$?; if [ "$code" -ne 0 ]; then printf "[vcpkg] 操作失败，退出码：%s\n" "$code" >&2; fi' EXIT

# 以脚本所在的 src 定位清单，不受调用时工作目录影响。
PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALL_ROOT="$PROJECT_ROOT/build/vcpkg_installed"
UPDATE_BASELINE=0

if [ "$#" -gt 1 ]; then
    echo '[vcpkg] 用法：vcpkg_install.sh [--update-baseline]' >&2
    exit 2
fi
case "${1-}" in
    '') ;;
    --update-baseline) UPDATE_BASELINE=1 ;;
    *) printf '[vcpkg] 不支持的参数：%s\n' "$1" >&2; exit 2 ;;
esac

if [ -z "${VCPKG_ROOT:-}" ]; then
    echo '[vcpkg] 未设置 VCPKG_ROOT，请将其设置为 vcpkg 仓库目录。' >&2
    exit 1
fi
VCPKG_EXE="$VCPKG_ROOT/vcpkg"
if [ ! -x "$VCPKG_EXE" ]; then
    printf '[vcpkg] 未找到可执行文件：%s，请先运行 bootstrap-vcpkg.sh。\n' "$VCPKG_EXE" >&2
    exit 1
fi

# 按当前进程系统与架构选择 triplet；Rosetta 下可能得到 x64-osx。
case "$(uname -s)" in
    Darwin) TARGET_OS=osx ;;
    Linux) TARGET_OS=linux ;;
    *) echo '[vcpkg] 仅支持 macOS 和 Linux。' >&2; exit 1 ;;
esac
case "$(uname -m)" in
    arm64|aarch64) TARGET_ARCH=arm64 ;;
    x86_64|amd64) TARGET_ARCH=x64 ;;
    *) echo '[vcpkg] 不支持当前 CPU 架构。' >&2; exit 1 ;;
esac
TRIPLET="$TARGET_ARCH-$TARGET_OS"

if [ "$UPDATE_BASELINE" -eq 1 ]; then
    command -v python3 >/dev/null 2>&1 || { echo '[vcpkg] 更新基线需要 Python 3。' >&2; exit 1; }
    command -v git >/dev/null 2>&1 || { echo '[vcpkg] 更新基线需要 Git。' >&2; exit 1; }
fi

printf '[vcpkg] 清单目录：%s\n[vcpkg] vcpkg 根目录：%s\n[vcpkg] 安装目录：%s\n[vcpkg] 目标平台：%s\n' \
    "$PROJECT_ROOT" "$VCPKG_ROOT" "$INSTALL_ROOT" "$TRIPLET"

# 同步可执行工具与本地仓库脚本版本，不会 git pull。
if [ -f "$VCPKG_ROOT/bootstrap-vcpkg.sh" ]; then
    echo '[vcpkg] 正在同步 vcpkg 可执行工具与仓库脚本版本...'
    sh "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi

if [ "$UPDATE_BASELINE" -eq 1 ]; then
    bash "$PROJECT_ROOT/update_vcpkg_configuration.sh" --no-pause
else
    echo '[vcpkg] 使用当前锁定基线。'
fi

echo '[vcpkg] 正在解析并安装依赖，此过程可能持续较长时间...'
# manifest-root 指向清单；install-root 固定依赖输出；triplet 指定目标平台。
# 首次构建可能需要联网下载源码，并持续较长时间。
"$VCPKG_EXE" install \
    "--x-manifest-root=$PROJECT_ROOT" \
    "--x-install-root=$INSTALL_ROOT" \
    --triplet "$TRIPLET"
echo '[vcpkg] 所有依赖均已安装完成。'
