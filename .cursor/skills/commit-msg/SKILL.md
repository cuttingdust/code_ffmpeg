---
name: commit-msg
description: Generate a Git commit message that includes a Linear issue key. Use when the user invokes /commit-msg or asks for a commit message for a LIN issue.
disable-model-invocation: true
---

# Commit Msg

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Generate a commit message.

1. Resolve `LIN-xxx` from the user message or current branch; ask if missing.
2. Use the format `feat(LIN-123): 描述` unless the change type clearly requires another conventional commit type such as `fix`, `test`, `docs`, or `refactor`.
3. Keep the subject concise and meaningful.
4. Do not commit automatically unless the user explicitly asks.
