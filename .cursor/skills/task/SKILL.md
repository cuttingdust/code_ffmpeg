---
name: task
description: Create or preview a normal Linear Task issue for concrete development work. Use when the user invokes /task or asks to create an implementation task.
disable-model-invocation: true
---

# Task

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a concrete development Task as a normal Issue.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Treat the task as a normal Linear Issue by default, not a Sub-issue.
3. Resolve Phase/Milestone from the user message, current plan context, or ask the user when needed.
4. If the user provides a related Requirement/Feature `LIN-xxx`, record it as a relation/label/description reference; do not require it as a parent.
5. Add labels such as `repo:slug`, `type:task`, and optional `feature:*`.
6. Build a preview with Type `Task`, title, Phase/Milestone, related Feature if any, labels, Team, and `project.linear_name`.
7. Ask for confirmation before calling Linear API.
8. After creation, provide the resulting `LIN-xxx` and suggest `/branch` or `/linear-start` when useful.
