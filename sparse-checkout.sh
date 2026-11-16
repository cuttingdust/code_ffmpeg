#!/usr/bin/env bash
# macOS/Linux 本地目录整理入口，克隆项目后执行一次：
#   bash sparse-checkout.sh           隐藏 Windows 脚本及 .exe/.dll/.sys 文件
#   bash sparse-checkout.sh --list    查看实际隐藏的文件
#   bash sparse-checkout.sh --show src/open.bat  恢复指定文件（可传多个路径或目录）
#   bash sparse-checkout.sh --disable 恢复显示全部已跟踪文件
#
# Git 仍保存完整历史；本脚本不删除仓库文件，也不提交或推送任何内容。
# 规则保存在本地 Git 元数据中，后续 pull 沿用；新 clone 需要再次执行。
# 此操作替换本地已有的稀疏规则，请先自行保存需要保留的自定义规则。
# 有未提交修改的文件可能被 Git 保留并提示；请先提交或妥善保存修改。
# 本脚本不负责清理未跟踪文件，也不会自动按扩展名清理磁盘内容。
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
command -v git >/dev/null 2>&1 || { echo '需要安装 Git。' >&2; exit 1; }
# 从脚本位置定位仓库，从其他目录调用也作用于本项目。
git -C "$SCRIPT_DIR" rev-parse --show-toplevel >/dev/null
REPO_ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)

usage() {
    cat <<'HELP'
用法：bash sparse-checkout.sh [选项]
  无参数                 设置默认隐藏规则，清除此前 --show 添加的例外
  --list                 列出当前实际隐藏的已跟踪文件
  --rules                显示当前稀疏检出规则
  --show 路径...         恢复指定已跟踪文件或目录（路径相对于仓库根目录）
  --disable              恢复显示全部已跟踪文件并关闭稀疏检出
  --help                 显示帮助
默认隐藏：.bat、.cmd、.ps1、.exe、.dll、.sys（后缀不区分大小写）。
--show 添加的例外会随 pull 保留；再次无参数运行会重设默认规则。
HELP
}

# Git 标记为 S（skip-worktree）且磁盘上不存在的条目，才计为实际隐藏。
# 使用 NUL 分隔，支持文件名含空格；修改后被 Git 保留的文件不会算入其中。
list_hidden() {
    local record path count=0
    while IFS= read -r -d '' record; do
        [[ "$record" == 'S '* ]] || continue
        path=${record:2}
        if [[ ! -e "$REPO_ROOT/$path" && ! -L "$REPO_ROOT/$path" ]]; then
            printf '  %s\n' "$path"
            count=$((count + 1))
        fi
    done < <(git -C "$REPO_ROOT" ls-files -t -z)
    printf '当前实际隐藏 %s 个已跟踪文件。\n' "$count"
}

case "${1-}" in
    --help|-h)
        usage
        ;;
    --list|--rules|--disable|'')
        if (( $# > 1 )); then usage >&2; exit 2; fi
        case "${1-}" in
            --list) list_hidden ;;
            --rules)
                if [[ "$(git -C "$REPO_ROOT" config --bool core.sparseCheckout || true)" == true ]]; then
                    git -C "$REPO_ROOT" sparse-checkout list
                else
                    echo '当前未启用稀疏检出。'
                fi
                ;;
            --disable)
                git -C "$REPO_ROOT" sparse-checkout disable
                echo '已恢复显示全部已跟踪文件。'
                ;;
            '')
                # 后出现的规则覆盖前面的规则；重设默认值会移除 --show 的例外。
                git -C "$REPO_ROOT" sparse-checkout set --no-cone \
                    '/*' '!*.[bB][aA][tT]' '!*.[cC][mM][dD]' \
                    '!*.[pP][sS]1' '!*.[eE][xX][eE]' \
                    '!*.[dD][lL][lL]' '!*.[sS][yY][sS]'
                echo '默认隐藏规则已设置；有本地修改的文件可能仍被 Git 保留。'
                echo '可运行 --list 查看实际隐藏文件，--show 路径 恢复指定内容。'
                ;;
        esac
        ;;
    --show)
        shift
        if (( $# == 0 )); then usage >&2; exit 2; fi
        # 先验证所有路径再修改规则；不支持通配符，文件名按字面值处理。
        # 只恢复 Git 已跟踪的内容，不能从历史中恢复已删除的文件。
        PATTERNS=()
        for path in "$@"; do
            while [[ "$path" == ./* ]]; do path=${path#./}; done
            path=${path%/}
            case "$path" in
                ''|.|/*|..|../*|*/../*|*/..|*$'\n'*)
                    printf '请使用仓库内的相对文件或目录路径：%s\n' "$path" >&2
                    exit 2 ;;
            esac
            if ! git -C "$REPO_ROOT" ls-files --error-unmatch -- ":(literal)$path" >/dev/null 2>&1; then
                printf '未找到已跟踪文件或目录：%s\n' "$path" >&2
                exit 2
            fi
            # 转义 gitignore 模式字符，避免文件名中的 [、*、? 被当成通配符。
            escaped=$(printf '%s\n' "$path" | sed 's/[][\\*?]/\\&/g')
            is_file=false
            while IFS= read -r -d '' tracked; do
                if [[ "$tracked" == "$path" ]]; then is_file=true; break; fi
            done < <(git -C "$REPO_ROOT" ls-files -z -- ":(literal)$path")
            if [[ "$is_file" == true ]]; then
                PATTERNS+=("/$escaped")
            else
                # 目录例外覆盖内部所有扩展名规则，也适用于以后 pull 的新文件。
                PATTERNS+=("/$escaped/**")
            fi
        done
        if [[ "$(git -C "$REPO_ROOT" config --bool core.sparseCheckout || true)" != true ]]; then
            echo '当前未启用稀疏检出，已跟踪文件无需通过稀疏规则恢复。'
            exit 0
        fi
        if [[ "$(git -C "$REPO_ROOT" config --bool core.sparseCheckoutCone || true)" == true ]]; then
            echo '当前使用 cone 模式；请先无参数运行本脚本设置文件级隐藏规则。' >&2
            exit 2
        fi
        printf '%s\n' "${PATTERNS[@]}" | git -C "$REPO_ROOT" sparse-checkout add --stdin
        echo '指定内容已加入显示例外，后续 pull 沿用；再次无参数运行会清除例外。'
        ;;
    *) usage >&2; exit 2 ;;
esac
