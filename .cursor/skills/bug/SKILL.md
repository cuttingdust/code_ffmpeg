---
name: bug
description: Create or preview a Linear Bug linked to a parent Story. Use when the user invokes /bug or asks to file a bug for the current repository.
disable-model-invocation: true
---

# Bug

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear Bug.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Resolve the parent Story in this order: user message `LIN-xxx`, current Git branch `LIN-xxx`, then ask the user.
3. Do not create a Bug without a parent Story.
4. Build a preview with Type `Bug`, title, parent issue, labels, Team, and `project.linear_name`.
5. Ask for confirmation before calling Linear API.
6. Keep one bug as one Linear Issue; do not hide bugs inside checklists.
