# Linear 工作流使用手册

这份文档说明如何使用旁边的 `linear-workflow.mdc` 规则。它的目标是把当前 Git 仓库、Linear Project、`docs/plan/`、分支、commit 和 PR 状态串起来，让一个需求从规划到合并都有清晰记录。

如果只是临时忘了命令，不需要翻完整文档，直接在 Agent 聊天里输入：

```text
/linear-help
```

它会显示命令速查、常用示例和当前仓库状态。

## 适用场景

当你在一个代码仓库里工作，并希望用 Linear 管理需求、任务、测试用例、Bug、分支和 PR 状态时，使用这套工作流。

典型场景：

- 为当前仓库初始化 Linear 项目绑定。
- 从 `docs/plan/` 批量创建 Epic、Story、Sub Task、Test。
- 创建需求、Bug、测试用例，并挂到正确的父 Story。
- 根据 Issue 生成分支名和 commit message。
- 在提交、同步或 PR 合并后更新 Linear 状态和 `docs/plan` 勾选。

## 核心概念

### 一仓一 Project

每个 Git 仓库绑定一个 Linear Project。绑定信息写在仓库根目录的 `.linear.yaml`，也就是和 `.git` 同级的位置。

规则要求：

- `team` 表示 Linear 里的产品线或团队，例如 `口扫`。
- `project.slug` 通常和 Git 仓库名一致，例如 `code_ffmpeg`。
- `project.title_cn` 是中文项目说明，例如 `音视频开发`。
- `project.linear_name` 是 Linear 里显示的项目名，默认格式是 `中文说明(仓库slug)`，例如 `音视频开发(code_ffmpeg)`。

### Issue 层级

`docs/plan/` 和 Linear 的对应关系如下：

| 文档层级 | Linear 类型 | 说明 |
| --- | --- | --- |
| `## 1. 模块` | Epic | 大模块或大方向 |
| `### 1.1 需求` | Story | 一个可交付需求 |
| `#### 1.1.1 开发任务` | Sub-issue / Task | Story 下的开发任务 |
| `#### TC: ...` | Sub-issue / Test | Story 下的测试用例 |
| 缺陷 | Bug | 通常挂到父 Story |

规则特别强调：不要用 Checklist 堆测试用例。一个测试用例应该是一个独立的 Sub Test Issue。

## 第一次使用

### 1. 打开目标代码仓库

先在 Cursor 中打开你要工作的 Git 仓库。规则只认当前打开仓库根目录下的 `.linear.yaml`。

如果你打开的是主仓，就配置主仓的 Project。如果打开的是子仓，就配置子仓自己的 Project。

### 2. 执行 `/linear-init`

当仓库还没有 `.linear.yaml` 时，输入：

```text
/linear-init
```

Agent 会按规则询问这些信息：

1. Linear Team 名，例如 `口扫`。
2. 仓库 slug，例如 `code_ffmpeg`。
3. 中文项目说明，例如 `音视频开发`。
4. 确认 Linear Project 显示名，例如 `音视频开发(code_ffmpeg)`。
5. 仓库角色，是 `main` 还是 `component`。
6. 绑定已有 Linear Project，还是新建 Linear Project。

注意：规则要求不能静默创建 Project，也不能猜 Team 或 Project。必须由你确认。

### 3. 查看绑定状态

初始化后可以输入：

```text
/linear-status
```

它会显示当前仓库绑定的：

- Team
- `project.slug`
- `project.linear_name`
- 是否启用自动同步
- 当前上下文里的 `LIN-xxx`

## 常用命令

### 初始化和开关

```text
/linear-init
```

无 `.linear.yaml` 时初始化当前仓库的 Linear 绑定。

```text
/linear-on
```

启用自动同步。

```text
/linear-off
```

关闭自动同步。关闭后，创建类命令仍可以输出结构化创建单，但不会自动调用 Linear API。

```text
/linear-status
```

查看当前 Linear 工作流配置状态。

### 创建需求

```text
/req 导出报表支持 CSV 和 Excel
```

通常创建一个 Story。创建前 Agent 会先给你预览：

- Type
- 标题
- 所属 Team
- 所属 Linear Project
- Labels
- 父 Issue，如果有
- 描述

你确认后才会真正创建。

示例结果可能类似：

```text
Type: Story
Title: [code_ffmpeg] 导出报表支持 CSV 和 Excel
Project: 音视频开发(code_ffmpeg)
Labels: repo:code_ffmpeg
```

### 创建子任务

如果你明确说这是某个 Story 的子任务，或提供父 Story 编号：

```text
/req LIN-100 增加导出接口参数校验
```

它会作为 `LIN-100` 下的 Sub Task 预览。

### 创建 Bug

```text
/bug LIN-100 导出 CSV 时中文乱码
```

Bug 必须关联父 Story。父 Story 的解析顺序是：

1. 用户消息里的 `LIN-xxx`
2. 当前 Git 分支名里的 `LIN-xxx`
3. 如果仍找不到，询问你

不要让 Agent 猜父 Story。

### 创建测试用例

```text
/tc LIN-100 CSV 导出字段完整性校验
```

这会创建一个挂在 `LIN-100` 下的 Sub Test。规则要求一个测试用例对应一个 Linear Issue。

更多测试用例示例：

```text
/tc LIN-100 空数据导出时生成只有表头的 CSV
/tc LIN-100 Excel 导出时保留金额格式
/tc LIN-100 导出失败时显示错误提示
```

### 创建 Epic

```text
/epic 报表系统重构
```

Epic 用于更大的模块或阶段，日常不一定常用。

## 分支和提交

### 生成分支名

```text
/branch LIN-123 导出报表
```

规则会根据配置里的 `defaults.branch_pattern` 生成推荐分支名，但不会自动创建分支。

示例：

```text
feat/LIN-123-export-report
```

你可以手动创建：

```bash
git checkout -b feat/LIN-123-export-report
```

### 生成 commit message

```text
/commit-msg LIN-123 完成 CSV 导出
```

示例输出：

```text
feat(LIN-123): 完成 CSV 导出
```

### 查看当前应该关联什么

```text
/link
```

它会根据当前分支、上下文和规则，提示当前应该对应哪个 `LIN-xxx`，以及推荐 commit 模板。

## 状态流转

### 开始开发

```text
/linear-start LIN-123
```

将对应 Issue 改为 `In Progress`。

### 进入 Review

```text
/linear-review LIN-123
```

将对应 Issue 改为 `In Review`。

### 手动完成

```text
/linear-done LIN-123
```

将对应 Issue 改为 `Done`。

注意：规则里说不要因为聊天结束就标 Done。完成应该以 commit 和 PR 合并规则为准。

## docs/plan 用法

### 推荐文件位置

默认规划目录是当前仓库：

```text
docs/plan/
```

具体路径以 `.linear.yaml` 里的 `planning.root` 为准。

### 推荐格式

示例：

```markdown
## 1. 报表导出

### 1.1 导出报表 LIN-100

- [ ] 1.1.1 后端 API LIN-101
- [ ] 1.1.2 前端导出按钮 LIN-102
- [ ] TC: CSV 字段完整 LIN-103
- [ ] TC: Excel 金额格式正确 LIN-104
```

带 `LIN-xxx` 的行可以被 `/sync` 或 Git 同步规则识别并勾选。

### 从 plan 批量创建 Linear Issue

```text
/plan-sync
```

它会解析 `docs/plan/` 中的三层结构：

- Epic
- Story
- Sub Task / Sub Test

然后先展示批量创建预览。你确认后才会创建 Linear Issue。

## 同步

### 手动同步

```text
/sync
```

它会根据当前分支、commit、用户说明或 `LIN-xxx`：

- 更新 Linear Issue 状态。
- 勾选 `docs/plan` 中对应 `LIN-xxx` 的任务行。
- 必要时提示你确认。

### commit 同步

如果 commit message 包含 `LIN-xxx`：

```text
feat(LIN-101): 完成导出接口
```

同步时会识别 `LIN-101`，并把 `docs/plan` 中含有 `LIN-101` 的行从：

```markdown
- [ ] 1.1.1 后端 API LIN-101
```

改为：

```markdown
- [x] 1.1.1 后端 API LIN-101
```

### PR 合并同步

当 PR 合并到主分支，并且 PR、分支或描述中包含 Story 的 `LIN-xxx` 时：

- Linear 中对应 Story 会变为 `Done`。
- `docs/plan` 中 Story 行会被勾选。
- 本 PR 完成的 Sub 行也会被勾选。

如果 PR 只包含 Sub Task 的 `LIN-xxx`，规则要求只勾选 Sub 行，不自动把 Story 标 Done。

## 多仓库使用

规则按当前打开的 Git 根目录工作。

例如：

- 主仓 `cad-app` 绑定 Project `口扫设计软件(cad-app)`。
- 子仓 `rendering` 绑定 Project `渲染引擎(rendering)`。
- 子仓 `code_ffmpeg` 绑定 Project `音视频开发(code_ffmpeg)`。

如果当前打开的是 `rendering`，但你说的需求明显属于 `cad-app`，Agent 应该询问：

```text
这个需求要记入哪个 Project？
```

不能擅自把 Issue 创建到另一个 Project。

## 没有 Linear API 时怎么办

如果没有 API Key，或 MCP/API 暂时不可用，创建类命令不会强行失败。规则要求输出结构化创建单，方便你手动粘贴到 Linear。

示例：

```text
Title: [code_ffmpeg] 导出 CSV 时中文乱码
Type: Bug
Team: 口扫
Project: 音视频开发(code_ffmpeg)
Parent: LIN-100
Labels: repo:code_ffmpeg
Description:
导出 CSV 后，中文字段在 Excel 中显示乱码。需要确认编码格式和下载响应头。
```

## API Key 安全

不要把 Linear API Key 写进会提交到 Git 的 `.linear.yaml`。

推荐方式：

```bash
set LINEAR_API_KEY=your_key_here
```

或使用本地文件：

```text
linear.yaml.local
```

这个文件应加入 `.gitignore`。

## 完整示例

### 示例 1：新仓库接入 Linear

```text
/linear-init
```

你回答：

```text
Team: 口扫
仓库 slug: code_ffmpeg
中文项目说明: 音视频开发
Linear Project 显示名: 音视频开发(code_ffmpeg)
仓库角色: component
绑定方式: 新建 Linear Project
```

然后查看：

```text
/linear-status
```

确认配置正确后开始创建需求：

```text
/req 支持视频导出进度展示
```

### 示例 2：为需求创建任务和测试

已有 Story：

```text
LIN-200 支持视频导出进度展示
```

创建开发任务：

```text
/req LIN-200 后端返回导出进度百分比
/req LIN-200 前端展示导出进度条
```

创建测试用例：

```text
/tc LIN-200 导出中显示实时进度
/tc LIN-200 导出失败时进度条停止并显示错误
/tc LIN-200 取消导出后状态恢复
```

### 示例 3：开发并提交

生成分支名：

```text
/branch LIN-201 后端返回导出进度百分比
```

假设输出：

```text
feat/LIN-201-export-progress-api
```

创建分支后开发，提交前生成 commit message：

```text
/commit-msg LIN-201 完成导出进度接口
```

输出：

```text
feat(LIN-201): 完成导出进度接口
```

提交后同步：

```text
/sync
```

### 示例 4：从 docs/plan 批量创建

先写规划：

```markdown
## 1. 视频导出

### 1.1 导出进度展示

- [ ] 1.1.1 后端进度接口
- [ ] 1.1.2 前端进度条
- [ ] TC: 导出中显示实时进度
- [ ] TC: 导出失败显示错误
```

然后执行：

```text
/plan-sync
```

Agent 会预览要创建的 Epic、Story、Sub Task、Sub Test。确认后创建，并把返回的 `LIN-xxx` 写回 plan。

## 常见问题

### 没有 `.linear.yaml` 能创建 Issue 吗？

不能直接自动创建。规则要求先执行 `/linear-init`。如果你不想初始化，只能输出创建单给你手动粘贴。

### 可以只说“帮我建个 bug”吗？

可以，但 Bug 必须有关联父 Story。如果消息和当前分支都没有 `LIN-xxx`，Agent 会问你父 Story 是哪个。

### `/branch` 会直接创建 Git 分支吗？

不会。它只输出推荐分支名。是否创建分支由你决定。

### 聊天结束会自动把 Linear 标 Done 吗？

不会。规则明确要求不要因为聊天结束就标 Done。Done 应该由 `/linear-done`、commit/PR 同步或 PR 合并触发。

### 测试用例能写在 Checklist 里吗？

不推荐。规则要求一用例一 Issue，也就是用 `/tc` 创建独立的 Sub Test。

### 当前仓库和需求所属 Project 不一致怎么办？

Agent 应该询问你要记到哪个 Project，不能猜。

## 推荐日常流程

1. 新仓库先 `/linear-init`。
2. 用 `/linear-status` 确认绑定。
3. 写或更新 `docs/plan/`。
4. 用 `/plan-sync` 批量创建需求和子任务，或用 `/req`、`/bug`、`/tc` 单独创建。
5. 开发前用 `/branch` 生成分支名。
6. 开始开发时用 `/linear-start`。
7. 提交时用 `/commit-msg`。
8. 提交或 PR 后用 `/sync`。
9. PR 合并后根据规则同步 Story Done 和 plan 勾选。
