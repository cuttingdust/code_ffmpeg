---
name: sync
description: Synchronize Linear issue status and docs/plan checkboxes from branch, commit, PR, or user-provided LIN issue context. Use when the user invokes /sync.
disable-model-invocation: true
---

# Sync

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Synchronize Linear and `docs/plan`.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and explain what would be updated manually.
2. Resolve relevant `LIN-xxx` keys from the user message, current branch, recent commits, or PR context.
3. Update only plan lines containing the resolved `LIN-xxx`; never modify unrelated checklist lines.
4. For Sub Task or Test completion, check the matching sub line only.
5. For merged PRs that reference a Story, mark the Story Done and check the Story line plus completed sub lines.
6. Preview changes before making broad updates.
