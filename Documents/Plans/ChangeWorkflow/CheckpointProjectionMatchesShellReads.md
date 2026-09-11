<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T00:10:12.783Z","dependsOn":[]} -->
# Fix: the checkpoint projection's `match` rows miss repository files main read through a shell command

## Context
The checkpoint isolation lens detects "main read a file a later brief lists"
from the projection's `match` rows
(`.agents/skills/next-plan-checkpoint-review/references/worker.md` step 12,
currently `:120-127`; line numbers move, so re-read the step). The emitter
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
builds those rows from `Read` records only — its `tool_use` branch adds a read
entry under `if ($element.name -eq 'Read' -and $element.input.file_path)` — so a
repository file main read through a shell command (`Bash`/`PowerShell` running
`cat`, `sed -n`, `Get-Content`, and similar) is never matched against a later
brief's `Governing paths`/`Scope`. Observed on a synthetic fixture: one `Bash`
file read that a later brief listed produced no `match` row. Step 12 therefore
ends with an instruction to judge those shell rows by hand, which leaves the
same deterministic path computation in prose that the change owning the emitter
was meant to remove.

This gap is the re-planning report that
`Documents/Plans/ChangeWorkflow/CheckpointProjectionEmitsPaths.md` `## Design`
requested: it directed its fix session to confirm whether shell rows that read a
file are distinguishable and, if not, to report it instead of inventing a
classifier inside the emitter. The fix session confirmed they are not: for a
shell tool call the emitter has only the tool name and the raw command string,
so classifying one as a file read means parsing arbitrary shell text. The
user-approved change was therefore narrowed to `Read`-tool reads, which is why
this is out of scope of that change rather than an unmet criterion of it.

## Design
Give the emitter a narrow, conservative shell-read classifier so the lens keeps
one detection mechanism.

The author's recommendation: in the `tool_use` branch, when the tool is a shell
tool and the command's leading token is in a fixed allowlist of read-only
commands — `cat`, `sed -n`, `head`, `tail`, `Get-Content`, `type` — take the
first path-like argument (a token containing `/` or `\`, with surrounding quotes
and trailing sentence punctuation stripped) as the read path and add it to the
same read list `Read` records feed, so it produces the existing
`<line> match <read path> read-at <read line>` row through the existing
lower-cased, separator-folded suffix comparison. Rationale: the allowlist is
closed and each listed command's first path argument is unambiguously the file
it reads, so no general shell parsing is needed; anything outside the allowlist
(pipelines, redirections, `grep`, `find`, scripts) simply yields no row, which
is exactly today's behavior and never a false finding. A missed exotic read
stays a miss rather than becoming a wrong match.

With the classifier in place, delete step 12's sentence that a `match` row
covers only `Read`-tool reads and that shell rows must be judged from their own
rows, and document the new row source in the script's header block. Leave the
lens's judgment of what qualifies as isolation untouched.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — the `tool_use` branch's read-collection step and the header's documented row
  shapes
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — step 12's
  isolation-report paragraph, the shell-row sentence only

## In scope
- In `Get-TranscriptProjection.ps1`: collecting a shell-command file read into
  the read list, and the header comment block documenting which rows produce
  `match` rows
- In `.agents/skills/next-plan-checkpoint-review/references/worker.md`: the
  sentence in step 12 that limits `match` rows to `Read`-tool reads and defers
  shell rows to manual judgment

## Out of scope
- The isolation rule itself, step 11's precision guard, and step 13's exclusions
- The `match` row shape, the brief-path extraction, and the path comparison rule
- The friction and context lenses, `Measure-SessionContext.ps1`, the
  `broken-engine-context-efficiency/v1` envelope, and the checkpoint's dispatch
  condition
- `/next-plan`, `/next-plan-review`, and their measurement rules
- Any C++ or GLSL source
- Any transcript path, transcript text, or machine-local path in the repository

## Risk tier and invariants
Expected Tier 2 under `.agents/references/risk-tiers.md`: scoped behavior of one
subsystem's tooling — one skill package's bundled script plus its reference. No
determinism, wire, serialization, threading, or trust surface is touched, and no
claim or landing mechanism changes. Escalate if the fix reaches
`Measure-SessionContext.ps1`, the envelope schema, or the checkpoint's dispatch
condition. Invariants: the projection still prints nothing for sidechain
records and still emits one row per tool call, tool result, main-assistant text
element, and string-content record; a shell command outside the allowlist adds
no read and so cannot produce a `match` row; `match` rows keep their existing
shape and comparison.

## Acceptance criteria
- A transcript in which main reads a repository file with an allowlisted shell
  command, followed by a brief listing that path, yields a `match` row naming
  that path and the shell row's line
- A shell command outside the allowlist, and an allowlisted command whose path
  no later brief lists, yield no `match` row
- `Read`-tool reads keep producing the same `match` rows as before, and every
  other row shape is unchanged
- Step 12 contains no instruction to judge shell file reads by hand, and the
  script header documents that shell reads feed `match` rows
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for the changed package

## Notes
`Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md` lists
`Get-TranscriptProjection.ps1` under its `## Out of scope`, so it does not own
this root cause and no `## Coordination` constraint is needed. The change that
made the emitter emit `match` rows completes in its own session and its Plan
file is deleted on landing, so this Plan carries no `dependsOn` edge.
