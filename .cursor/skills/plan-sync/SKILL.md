---
name: plan-sync
description: Push docs/plan planning content into Linear by parsing Phase/Milestone sections and normal Requirement, Task, Bug, and Test issues. Use when the user invokes /plan-sync or asks to create Linear issues from plan documents.
disable-model-invocation: true
---

# Plan Sync

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Synchronize `docs/plan -> Linear`.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and tell the user to run `/linear-init` or enable sync.
2. Locate the planning root from `.linear.yaml` (`planning.root`), defaulting to `docs/plan/`.
3. Parse plan documents using the workflow structure:
   - `## Phase ...` sections become Linear Project Milestone candidates.
   - `### Feature ...` sections become Requirement issue candidates only when they need a `LIN-xxx`.
   - `Task:` lines become normal Task issues.
   - `Bug:` lines become normal Bug issues.
   - `Test:` or `TC:` lines become normal Test issues.
4. Detect existing `LIN-xxx` keys and do not recreate those issues.
5. Build a batch preview showing Type, title, Phase/Milestone, related Feature if any, labels, Team, and `project.linear_name`.
6. Ask for confirmation before calling Linear API.
7. After creation, write returned `LIN-xxx` keys back to the matching plan lines.
8. Preserve user-written plan content and comments; do not rewrite unrelated prose.
9. Do not create Sub-issues unless the user explicitly requested parent/sub-issue behavior.
