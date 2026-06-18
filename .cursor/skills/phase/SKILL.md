---
name: phase
description: Create or preview a Linear Project Milestone used as a development Phase. Use when the user invokes /phase or wants to split work into phases.
disable-model-invocation: true
---

# Phase

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Create or preview a Linear Project Milestone as a Phase.

1. Read `.linear.yaml`; if missing or disabled, output a structured creation form instead of calling Linear API.
2. Use the configured Linear Project from `.linear.yaml`; do not guess another Project.
3. Build a preview with Phase name, description, target date if provided, Team, and `project.linear_name`.
4. Ask for confirmation before calling Linear API.
5. After creation, provide the milestone identifier/name and suggest adding or updating the matching `## Phase ...` section in `docs/plan`.
