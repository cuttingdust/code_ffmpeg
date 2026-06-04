---
name: linear-status
description: Show the current repository's Linear workflow status. Use when the user invokes /linear-status or asks what Linear Team, Project, enabled state, or current LIN issue applies.
disable-model-invocation: true
---

# Linear Status

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Report the Linear workflow status for the current Git repository.

1. Read `.linear.yaml` from the repository root.
2. If it is missing, tell the user to run `/linear-init`.
3. Show `team`, `project.slug`, `project.linear_name`, `project.title_cn`, `repo.slug`, `enabled`, planning root, sync settings, and current `LIN-xxx` if one can be resolved from the branch or user message.
4. Do not create or update Linear data.
