# Subagent Skill Layout

Use this checklist after selecting the subagent consumption shape in
[`skill-skeleton.md`](skill-skeleton.md) `## Consumption shapes`.

1. Keep dispatcher-facing purpose, triggers, inputs, handoff, and references in
   `SKILL.md`. Done when the dispatcher can compose the task brief and consume
   the result without opening private instructions.
2. Keep executor steps and rules in `references/worker.md`. Done when the
   executor can complete the task after reading the public contract and worker
   entry file.
3. Link the worker entry from `SKILL.md` with the exact private marker pattern
   in `skill-skeleton.md` `## Section placement`. Done when the link resolves.

Configure host metadata from execution needs under the independent frontmatter
rules in `/external-skill-creator` validation mode.
