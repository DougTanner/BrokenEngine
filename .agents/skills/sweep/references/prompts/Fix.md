You are the FIX step of one unit pipeline in a sweep run by `.agents/skills/sweep/SKILL.md`.
Repository root (session worktree): {{ROOT}}
Sweep Plan: {{PLAN}}

Read `{{TYPE}}` sections `## Find rules` and `## Fix bound`, and `{{PLAN}}` section `## Sweep rules` when `## Find rules` defers to it. A FIND agent already recorded this unit's findings in `{{FINDINGS}}`; read that file.

Unit files (the only files you may edit):
{{FILES}}

Rules:
- Other agents are editing other units in this same worktree at the same time. Edit only the unit files above. Never run a Git command that changes state (no add, commit, checkout, restore, stash, reset). Do not build.
- For each `local` finding: confirm it against the unit files and the rules `## Find rules` names (a permitted form it names is not a violation), then apply it if it is a real violation and the fix stays within `## Fix bound`; otherwise decline it with a one-line reason. For a rename, search the repository (C++, shaders, and AGENTS.md or docs) first; if any reference lies outside the unit, do not apply it — reclassify it `cross-file`.
- Do not apply `cross-file` or `out-of-bound` findings; carry them to your output unchanged (with any reclassification noted).
- Preserve each file's encoding, BOM, line endings, tabs, and trailing newline exactly; change only the bytes the fix needs.
- After editing, read `git diff HEAD -- <unit files>` and confirm every hunk belongs to an applied finding and stays within `## Fix bound`.

Your final message is written verbatim to a file the coordinator parses. Format:

# Fix: {{UNIT}}

## Applied
- F<n> `path:line` short description   (or `none`)

## Declined
- F<n> | `path:line` | Rule <N> | reason   (or `none`)

## Cross-file
- F<n> | `path:line` | Rule <N> | violating text | exact fix, with old -> new names for renames   (or `none`)

## Out-of-bound
- F<n> | `path:line` | Rule <N> | why it is out of bound   (or `none`)

End with one line: `FIX-DONE applied=<n> declined=<n> crossfile=<n> oob=<n>`.
