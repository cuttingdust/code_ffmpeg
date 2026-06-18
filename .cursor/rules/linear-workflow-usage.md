# Linear 工作流使用手册

这套工作流现在采用更扁平的模型：

```text
Linear Project = 当前仓库
Phase / Milestone = 开发阶段
Issue = 真正执行的需求、任务、Bug、测试用例
```

默认不使用 Linear Sub-issue。只有你明确说“挂父 Issue”或“创建子任务”时，才使用 parent/sub-issue。

如果忘了命令，直接输入：

```text
/linear-help
```

## 核心模型

推荐结构：

```text
Project: 音视频开发(code_ffmpeg)
  Phase 1: 导出基础能力
    LIN-101 Requirement: 导出报表
    LIN-102 Task: 实现导出 API
    LIN-103 Bug: 修复 CSV 中文乱码
    LIN-104 Test: CSV 字段完整性校验
```

对应 `docs/plan`：

```markdown
## Phase 1: 导出基础能力  MS: phase-1

### Feature: 导出报表  LIN-101

- [ ] Task: 实现导出 API  LIN-102
- [ ] Bug: 修复 CSV 中文乱码  LIN-103
- [ ] Test: CSV 字段完整性校验  LIN-104
```

## 初始化

新仓库先执行：

```text
/linear-init
```

它会创建或确认当前仓库根目录的 `.linear.yaml`，绑定：

- Linear Team
- Linear Project
- 仓库 slug
- 中文项目说明
- `docs/plan` 路径
- 同步配置

查看当前配置：

```text
/linear-status
```

## 创建命令

创建阶段：

```text
/phase Phase 1 导出基础能力
```

创建需求记录：

```text
/req 导出报表
```

创建真实开发任务：

```text
/task LIN-101 实现导出 API
```

创建修复任务：

```text
/bug LIN-101 CSV 导出中文乱码
```

创建测试用例：

```text
/tc LIN-101 CSV 字段完整性校验
```

说明：

- `/phase` 创建或预览 Linear Milestone。
- `/req` 创建普通 Requirement Issue。
- `/task` 创建普通 Task Issue。
- `/bug` 创建普通 Bug Issue。
- `/tc` 创建普通 Test Issue，一用例一 Issue。
- `LIN-101` 这类编号只作为 Feature/Requirement 关联，不默认作为父 Issue。

## 同步命令

三个同步命令操作同一个 `docs/plan` 文档体系，但方向不同：

```text
/plan-sync    docs/plan -> Linear
/linear-pull  Linear -> docs/plan
/sync         开发进度同步
```

### /plan-sync

把本地规划推到 Linear。

适合你先写好了 `docs/plan`，然后想批量创建 Linear Milestone 和普通 Issue。

它会：

- 解析 `## Phase` 为 Milestone。
- 解析 `### Feature` 为 Requirement Issue（如果需要编号）。
- 解析 `Task:`、`Bug:`、`Test:` 为普通 Issue。
- 跳过已经有 `LIN-xxx` 的行，避免重复创建。
- 创建后把返回的 `LIN-xxx` 写回同一份 plan。

### /linear-pull

把 Linear 中已有内容拉回 `docs/plan`。

适合你以前已经在 Linear 里做过规划，但本地没有文档，或者想把历史任务补进 plan。

它会：

- 读取当前 `.linear.yaml` 绑定的 Project。
- 拉取 Milestone 和普通 Issue。
- 按 Phase/Feature/Issue 结构预览写入 `docs/plan`。
- 已有相同 `LIN-xxx` 的行只更新明确字段。
- 不删除本地文档里 Linear 找不到的内容，只报告差异。

### /sync

同步开发过程中的完成状态。

适合你已经有 `LIN-xxx`，并且代码提交、PR 或用户说明表明某些 Issue 完成。

它会：

- 根据分支、commit、PR 或用户说明解析 `LIN-xxx`。
- 更新 Linear Issue 状态。
- 勾选 `docs/plan` 中对应行。
- 只处理明确引用的 Issue，不推断整个 Phase 完成。

## 推荐流程

### 新项目

```text
/linear-init
/phase Phase 1 基础能力
/req 核心功能
/task LIN-101 实现核心接口
/tc LIN-101 核心接口正常返回
/plan-sync
```

### 已有 Linear，后来补文档

```text
/linear-pull
```

然后人工整理 `docs/plan`，再执行：

```text
/plan-sync
```

这样新写的、没有 `LIN-xxx` 的计划会补到 Linear，已有编号不会重复创建。

### 日常开发

```text
/branch LIN-102 实现导出 API
/linear-start LIN-102
/commit-msg LIN-102 完成导出 API
/sync LIN-102
```

## 多仓库注意事项

打开哪个 Git 仓库，就只读取该仓库根目录的 `.linear.yaml`。

如果一个需求明显属于另一个仓库或 Project，Agent 必须询问，不得猜测。

## API Key 安全

不要把 Linear API Key 写进会提交到 Git 的 `.linear.yaml`。

推荐使用：

```text
LINEAR_API_KEY
```

或本地忽略文件：

```text
linear.yaml.local
```
