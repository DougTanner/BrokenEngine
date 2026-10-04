You are the PROPAGATE step of a sweep run by `.agents/skills/sweep/SKILL.md`, for batch `{{BATCH}}`.
Repository root (session worktree): {{ROOT}}
Sweep Plan: {{PLAN}}

Read `{{TYPE}}` sections `## Find rules` and `## Fix bound`, and `{{PLAN}}` section `## Sweep rules` when `## Find rules` defers to it. The batch's per-unit FIX agents left these `cross-file` findings unapplied, collected in `{{CROSSFILE}}`; read that file.

For each finding: confirm it against the code and the rules `## Find rules` names; apply it if it is a real violation and its fix stays within `## Fix bound`, otherwise decline it with a one-line reason. Then follow `.agents/skills/update-affected-code/SKILL.md` over the applied changes: search the whole repository (C++, shaders, and AGENTS.md / docs that name the identifier) for every renamed or changed symbol and update every reference. Never edit `ThirdParty/` (AGENTS.md: do not modify); decline a finding whose fix needs a `ThirdParty/` edit. You cannot write `.agents/`; decline a finding whose fix needs an `.agents/` edit. Preserve encoding, BOM, line endings, tabs, and trailing newline. Never run a Git command that changes state and do not build.

Your final message is written verbatim to a file the coordinator reads. Format:

# Propagate: {{BATCH}}

## Applied
- F-id `path:line` old -> new, files updated   (or `none`)

## Declined
- F-id | `path:line` | Rule <N> | reason   (or `none`)

End with one line: `PROPAGATE-DONE applied=<n> declined=<n>`.
