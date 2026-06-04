---
name: tc
description: Create or preview a Linear Test sub-issue for a parent Story. Use when the user invokes /tc or asks to add a test case to Linear.
disable-model-invocation: true
---

# TC

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear test case.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Resolve the parent Story in this order: user message `LIN-xxx`, current Git branch `LIN-xxx`, then ask the user.
3. Do not create a test case without a parent Story.
4. Create one test case as one Sub Test Issue.
5. Do not use a checklist to store multiple test cases.
6. Build a preview and ask for confirmation before calling Linear API.
