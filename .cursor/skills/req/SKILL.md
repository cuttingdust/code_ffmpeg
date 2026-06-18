---
name: req
description: Create or preview a Linear Requirement issue for the current repository. Use when the user invokes /req or asks to record a feature, requirement, or product need.
disable-model-invocation: true
---

# Req

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear Requirement as a normal Issue.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and output a structured creation form.
2. Treat the requirement as a normal Linear Issue by default, not a Sub-issue.
3. Resolve Phase/Milestone from the user message, current plan context, or ask the user when needed.
4. Add labels such as `repo:slug`, `type:requirement`, and optional `feature:*`.
5. Build a preview with Type, title, Phase/Milestone, labels, Team, and `project.linear_name`.
6. Ask for confirmation before calling Linear API.
7. After creation, provide the resulting `LIN-xxx` and suggest `/task`, `/branch`, or `/linear-start` when useful.
