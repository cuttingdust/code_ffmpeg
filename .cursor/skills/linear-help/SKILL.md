---
name: linear-help
description: Show help for the repository Linear workflow, including available commands, common examples, setup steps, and where to find the detailed usage guide. Use when the user invokes /linear-help or asks how to use the Linear workflow.
disable-model-invocation: true
---

# Linear Help

Follow `.cursor/rules/linear-workflow.mdc`.

## Instructions

Give the user a concise help menu for the Linear workflow.

1. Start with the current repository status if it is quick to determine:
   - If `.linear.yaml` exists, mention the configured Team, Project, and `enabled` value.
   - If `.linear.yaml` is missing, tell the user to start with `/linear-init`.
2. Show the main commands grouped by task:
   - Setup: `/linear-init`, `/linear-status`, `/linear-on`, `/linear-off`.
   - Create: `/req`, `/bug`, `/tc`, `/epic`.
   - Development: `/branch`, `/commit-msg`, `/link`.
   - Status: `/linear-start`, `/linear-review`, `/linear-done`.
   - Sync: `/sync`, `/plan-sync`.
3. Include 3-5 practical examples, such as:
   - `/req 支持导出报表`
   - `/bug LIN-100 导出 CSV 中文乱码`
   - `/tc LIN-100 CSV 字段完整性校验`
   - `/branch LIN-123 导出报表`
   - `/sync LIN-123`
4. Mention the detailed guide if present: `.cursor/rules/linear-workflow-usage.md`.
5. Keep the answer short enough to use as an in-chat cheat sheet.
