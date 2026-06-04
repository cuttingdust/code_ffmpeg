---
name: req
description: Create or preview a Linear requirement Story or Sub Task for the current repository. Use when the user invokes /req or asks to create a requirement or development task.
disable-model-invocation: true
---

# Req

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear requirement.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and output a structured creation form.
2. Determine whether the request is a Story or a Sub Task under a parent Story.
3. For Sub Task creation, resolve the parent Story from the user message, current branch, or ask the user.
4. Build a preview with Type, title, parent issue, labels, Team, and `project.linear_name`.
5. Ask for confirmation before calling Linear API.
6. After creation, provide the resulting `LIN-xxx` and suggest `/branch` or `/linear-start` when useful.
