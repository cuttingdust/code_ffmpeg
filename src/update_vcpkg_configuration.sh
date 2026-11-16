#!/usr/bin/env bash
# 更新注册表基线，需要 Python 3 和 Git；默认不拉取本地 vcpkg。
# 用法：./update_vcpkg_configuration.sh [--pull-vcpkg] [--default-from-remote]
# 同时兼容 .bat 的 -PullVcpkg、-DefaultFromRemote、--no-pause 参数。
# 只修改同目录注册表配置，不安装依赖。baseline 是注册表的 Git 提交号。
# VCPKG_ROOT 必须指向 Git 检出的 vcpkg 仓库；需要 Python 3 和 Git。
# 两个更新参数可以组合；基线变化可能导致重编译或 API 兼容性变化。
set -euo pipefail

trap 'code=$?; if [ "$code" -ne 0 ]; then printf "[vcpkg-config] 更新失败，退出码：%s\n" "$code" >&2; fi' EXIT
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PULL_VCPKG=0
DEFAULT_FROM_REMOTE=0
for argument in "$@"; do
    case "$argument" in
        --pull-vcpkg|-PullVcpkg) PULL_VCPKG=1 ;;
        --default-from-remote|-DefaultFromRemote) DEFAULT_FROM_REMOTE=1 ;;
        --no-pause) ;; # Shell 版本始终不暂停。
        *) printf '[vcpkg-config] 不支持的参数：%s\n' "$argument" >&2; exit 2 ;;
    esac
done
if [ -z "${VCPKG_ROOT:-}" ]; then
    echo '[vcpkg-config] 未设置 VCPKG_ROOT。' >&2
    exit 1
fi
command -v python3 >/dev/null 2>&1 || { echo '[vcpkg-config] 需要 Python 3。' >&2; exit 1; }
command -v git >/dev/null 2>&1 || { echo '[vcpkg-config] 需要 Git。' >&2; exit 1; }

# 使用参数传递路径，支持空格；带引号的 heredoc 禁止 Shell 展开 Python 内容。
python3 - "$SCRIPT_DIR/vcpkg-configuration.json" "$VCPKG_ROOT" "$PULL_VCPKG" "$DEFAULT_FROM_REMOTE" <<'PY'
import json
import pathlib
import subprocess
import sys

path = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2])
if not (root / ".git").exists():
    raise SystemExit(f"[vcpkg-config] VCPKG_ROOT 不是 Git 检出目录：{root}")
config = json.loads(path.read_text(encoding="utf-8"))


# 只查询远端 HEAD，不修改本地仓库；查询失败保留旧基线，无旧值则报错。
def remote_head(repository, current):
    try:
        result = subprocess.check_output(
            ["git", "ls-remote", repository, "HEAD"], text=True
        ).split()
        if not result:
            raise ValueError("远端 HEAD 为空")
        return result[0]
    except (subprocess.CalledProcessError, ValueError) as error:
        if not current:
            raise
        print(f"[vcpkg-config] 警告：查询 {repository} 失败，保留原基线 {current}：{error}", file=sys.stderr)
        return current


# 仅允许快进拉取，分叉时失败，不自动合并。
if sys.argv[3] == "1":
    subprocess.run(["git", "-C", str(root), "pull", "--ff-only"], check=True)
default = config["default-registry"]
old = default.get("baseline", "")
if sys.argv[4] == "1":
    baseline = remote_head(
        default.get("repository") or "https://github.com/microsoft/vcpkg.git", old
    )
else:
    baseline = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    default.pop("repository", None)
default["kind"] = "builtin"
default["baseline"] = baseline
print(f"[vcpkg-config] 内置注册表：{old} -> {baseline}")
# 自定义 Git 注册表分别查询远端；其他类型保持不变。
for registry in config.get("registries", []):
    if registry.get("kind") != "git":
        continue
    old = registry.get("baseline", "")
    registry["baseline"] = remote_head(registry["repository"], old)
    print(f"[vcpkg-config] {registry['repository']}：{old} -> {registry['baseline']}")

# 仅修改基线相关字段，保留 overlay 和其他配置。
# 所有查询完成后才写入；保留其他 JSON 字段，整体以 UTF-8 无 BOM 重新排版。
path.write_text(json.dumps(config, ensure_ascii=False, indent="\t") + "\n", encoding="utf-8")
print(f"[vcpkg-config] 已更新：{path}")
PY
