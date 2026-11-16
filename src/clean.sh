#!/usr/bin/env bash
# 完整重建构建目录，与 clean.bat 的“删除后重新配置”行为一致。
# 用法：bash src/clean.sh [CMake 配置参数...]
# 示例：bash src/clean.sh -G Xcode
# 注意：运行即删除 src/build 内的工程、缓存、目标文件和 vcpkg_installed！
# 旧 CMake 参数随缓存一起删除，需要的 -G/-D 参数请重新传入。
# 不删除源码、仓库根目录 out 或 VCPKG_ROOT 公共工具仓库。
# 重新配置可能触发 vcpkg 下载和依赖重编译；不自动打开 IDE 或编译项目。
set -euo pipefail
trap 'code=$?; if (( code != 0 )); then printf "[clean] 执行失败，退出码：%s\n" "$code" >&2; fi' EXIT

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
BUILD_DIR="$SCRIPT_DIR/build"
# 删除前先检查工具，避免 CMake 都未安装时丢失已有构建目录。
command -v cmake >/dev/null 2>&1 || { echo '[clean] 请先安装 CMake。' >&2; exit 1; }
for argument in "$@"; do
    case "$argument" in
        -S*|-B*|--build|--open|--install|--workflow|--preset*|-P*|-E|--)
            printf '[clean] 不支持覆盖执行模式或目录的参数：%s\n' "$argument" >&2
            exit 2 ;;
    esac
done

# 删除路径只能由本脚本所在目录派生，不能通过参数指定。
# 拒绝符号链接，避免对外部构建目录做清理或后续重新配置。
if [[ -L "$BUILD_DIR" ]]; then
    echo '[clean] build 是符号链接，拒绝清理；请先检查其指向。' >&2
    exit 1
fi
if [[ -e "$BUILD_DIR" && ! -d "$BUILD_DIR" ]]; then
    echo '[clean] build 存在但不是目录，拒绝清理。' >&2
    exit 1
fi
if [[ -d "$BUILD_DIR" ]]; then
    printf '[clean] 正在删除：%s\n' "$BUILD_DIR"
    rm -rf -- "$BUILD_DIR"
else
    echo '[clean] 构建目录不存在，无需删除。'
fi

# 共用配置入口，避免 clean 和 update 的配置行为逐渐不一致。
bash "$SCRIPT_DIR/update.sh" "$@"
echo '[clean] 构建目录重新创建完成。'
