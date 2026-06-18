---
name: bug
description: Create or preview a normal Linear Bug issue for the current repository. Use when the user invokes /bug or asks to file a bug or repair task.
disable-model-invocation: true
---

# Bug

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear Bug as a normal Issue.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Treat the bug as a normal Linear Issue by default, not a Sub-issue.
3. Resolve Phase/Milestone from the user message, current plan context, or ask the user when needed.
4. If the user provides a related Requirement/Feature `LIN-xxx`, record it as a relation/label/description reference; do not require it as a parent.
5. Build a preview with Type `Bug`, title, Phase/Milestone, related Feature if any, labels, Team, and `project.linear_name`.
6. Ask for confirmation before calling Linear API.
7. Keep one bug as one Linear Issue; do not hide bugs inside checklists.
