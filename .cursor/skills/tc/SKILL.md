---
name: tc
description: Create or preview a normal Linear Test issue for the current repository. Use when the user invokes /tc or asks to add a test case to Linear.
disable-model-invocation: true
---

# TC

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear Test as a normal Issue.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Treat the test case as a normal Linear Issue by default, not a Sub-issue.
3. Resolve Phase/Milestone from the user message, current plan context, or ask the user when needed.
4. If the user provides a related Requirement/Feature `LIN-xxx`, record it as a relation/label/description reference; do not require it as a parent.
5. Create one test case as one Test Issue.
6. Do not use a checklist to store multiple test cases.
7. Build a preview and ask for confirmation before calling Linear API.
