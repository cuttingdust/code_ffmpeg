---
name: sync
description: Synchronize development progress for existing Linear issue keys, updating Linear status and docs/plan checkboxes from branch, commit, PR, or user-provided LIN context. Use when the user invokes /sync.
disable-model-invocation: true
---

# Sync

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Synchronize development progress for already-linked Linear issues.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and explain the manual updates.
2. Resolve relevant `LIN-xxx` keys from the user message, current branch, recent commits, or PR context.
3. Treat `/sync` as status/progress sync only; do not bulk-create missing Linear issues from plan. Use `/plan-sync` for `docs/plan -> Linear`.
4. Do not rebuild plan from Linear. Use `/linear-pull` for `Linear -> docs/plan`.
5. Update only plan lines containing resolved `LIN-xxx`; never modify unrelated checklist lines.
6. For Sub Task or Test completion, check the matching sub line only.
7. For merged PRs that reference a Story, mark the Story Done and check the Story line plus completed sub lines.
8. Preview changes before making broad updates.
