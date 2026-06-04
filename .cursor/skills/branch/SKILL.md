---
name: branch
description: Generate a recommended Git branch name from a Linear issue. Use when the user invokes /branch or asks for a branch name for a LIN issue.
disable-model-invocation: true
---

# Branch

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Generate a recommended branch name.

1. Resolve `LIN-xxx` from the user message or current branch; ask if missing.
2. Read `.linear.yaml` for `defaults.branch_pattern` if present.
3. Output the recommended branch name only; do not automatically create or switch branches unless the user explicitly asks.
4. Prefer a concise English slug after the issue number, for example `feat/LIN-123-export-report`.
