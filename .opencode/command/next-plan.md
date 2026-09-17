---
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli and presents the resolved Plan and execution card for implementation approval.
agent: manager
model: opencode/union-alpha
subtask: false
---

Run the `/next-plan` skill workflow for the user's request and arguments:

- User request: $ARGUMENTS
- Execute the next-plan skill workflow (.agents/skills/next-plan/SKILL.md) exactly as it defines, with no workflow changes and no argument reinterpretation.
