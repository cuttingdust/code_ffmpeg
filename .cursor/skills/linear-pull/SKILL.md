---
name: linear-pull
description: Pull Linear Project issues into docs/plan, creating or updating plan entries from Linear Epic, Story, Sub Task, Test, and Bug data. Use when the user invokes /linear-pull or asks to sync Linear tasks back to documentation.
disable-model-invocation: true
---

# Linear Pull

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Synchronize `Linear -> docs/plan`.

1. Read `.linear.yaml`; if missing or disabled, do not call Linear API and tell the user to run `/linear-init` or enable sync.
2. Use `team` and `project.linear_name` / `project.slug` from `.linear.yaml`; do not guess another Project.
3. Fetch visible Linear issues for the configured Project, including Epic, Story, Sub Task, Test, Bug, status, parent, labels, and identifiers.
4. Map Linear hierarchy into plan structure:
   - Epic -> `##`
   - Story -> `###`
   - Sub Task -> `- [ ]` or `- [x]` task line
   - Sub Test -> `- [ ] TC:` or `- [x] TC:` line
   - Bug -> bug line under the parent Story, or an "Untriaged Bugs" section if no parent exists.
5. If a plan line with the same `LIN-xxx` exists, update status checkboxes and cautiously update the title when the match is clear.
6. If a Linear issue is missing from the plan, preview where it will be inserted.
7. Do not delete plan content just because it is missing from Linear; report it as "not found in Linear" instead.
8. Ask for confirmation before editing plan files.
