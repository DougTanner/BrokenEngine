You are the FIND step of one unit pipeline in a sweep run by `.agents/skills/sweep/SKILL.md`.
Repository root (session worktree): {{ROOT}}
Sweep Plan: {{PLAN}}

Read `{{TYPE}}` sections `## Find rules` and `## Fix bound`, and `{{PLAN}}` section `## Sweep rules` when `## Find rules` defers to it. This FIND step only finds and records; a separate FIX agent applies the fixes afterwards from your output alone, and there is no spot-check. You are findings-only: never edit any file and never run a Git command that changes state.

Unit files (the whole scope of this step):
{{FILES}}

Scanner path arguments: {{PATHARGS}}

Steps:
1. Find every violation in the unit files by the procedure and rule set `## Find rules` gives. A permitted form it names is never a finding.
2. Classify each finding: `local` (the fix edits only unit files, including a rename whose every reference is inside the unit — confirm by searching the repository: C++, shaders, and AGENTS.md or docs), `cross-file` (a rename or signature change with references outside the unit), or `out-of-bound` (per `## Fix bound`).

Your final message is written verbatim to the findings file the FIX agent reads, so it must be self-contained. Format:

# Findings: {{UNIT}}
Scanner: <ran, N rows | could not run: reason | not used by this sweep type>

## Findings
One row per finding:
- F<n> | `path:line` | Rule <N> | class | violating text (verbatim, short) | exact fix (replacement text or precise edit; for renames old -> new and every in-unit reference line)

## Rejected scanner rows
- `path:line` | Rule <N> | why it is not a violation (one line each)

Write `none` under a heading that has no rows. No other prose. End with one line: `FIND-DONE findings=<n>`.
