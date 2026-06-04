---
name: linear-init
description: Initialize the current repository's Linear workflow configuration. Use when the user invokes /linear-init or wants to create .linear.yaml for the repository.
disable-model-invocation: true
---

# Linear Init

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Initialize the current Git repository for the Linear workflow.

1. Check whether `.linear.yaml` exists at the repository root.
2. If it exists, summarize the current config and ask before changing it.
3. If it does not exist, ask the init questionnaire from the rule:
   - Linear Team name.
   - Repository slug.
   - Chinese project title `title_cn`.
   - Confirm `project.linear_name` as `{title_cn}({slug})`.
   - Repository role: `main` or `component`; if `component`, ask `main_repo_slug`.
   - Bind existing Linear Project or create a new one.
4. Do not guess Team or Project.
5. Do not call Linear API until the user confirms the preview.
6. Never write API keys into committed config files.
