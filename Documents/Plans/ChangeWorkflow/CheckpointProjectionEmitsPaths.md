<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T23:03:27.711Z","dependsOn":[]} -->
# Fix: the checkpoint isolation lens computes path matching in prose over a truncated projection

## Context
A `/progressive-disclosure-review` of the change that added the governing-path
detection to the checkpoint isolation lens reported this as an out-of-scope
residual. The new detection clause sits in step 12's isolation-report paragraph
of `.agents/skills/next-plan-checkpoint-review/references/worker.md` (currently
`:114-129`, with the path-matching sentences at `:120-128`; it is cited, not
restated here). That prose prescribes a deterministic computation over
machine-readable inputs: for each in-span `use Agent` brief, take the
`Governing paths` and `Scope` path lists; take main's read paths from the span's
`use Read` rows and any shell row that reads a file; normalize both sides to
repository-relative form regardless of separator; and test membership. It also
prescribes a manual workaround for a projection limit — opening a row whose
summary the 160-character input-summary cap truncated to recover its path. That
cap is declared by the emitter itself at
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:4`
(`` `<line> use <tool> <input summary capped at 160 chars>` ``).

`.agents/skills/progressive-disclosure-review/references/worker.md` step 3
assigns exactly this shape — changed skill prose that computes a repeatable
verdict from explicit machine-readable inputs through path parsing and set
operations — to the script or reference that owns the computation. Here the
owning component is the projection emitter, which already parses every record
the clause reasons about and already truncates the one field the clause needs
whole.

The residual was excluded from the change that introduced the clause because the
claimed Plan `Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md`
listed `Get-TranscriptProjection.ps1` under its `## Out of scope`, so the clause
had to be written against the projection as it stands.

Session provenance (machine-local; not reproducible after cleanup). This session
observed the residual and records it, so the observing and recording sessions are
the same:
- Client: claude
- Conversation session ID: a9e74f3d-2914-4a8f-ad51-3a671350b4a6
- Worktree/branch UUID: 2f9bfe84-cdc3-4888-9c11-272dcd151967
- Session branch: claude/2f9bfe84-cdc3-4888-9c11-272dcd151967
- Worktree: .claude/worktrees/BrokenEngine/2f9bfe84-cdc3-4888-9c11-272dcd151967
- Landing ref: the session branch above; the clause this Plan builds on lands
  with that session's change, so this Plan carries no `dependsOn` edge.

## Design
Move the machine part of the detection into the emitter and leave the judgment in
the lens.

The author's recommendation is to have `Get-TranscriptProjection.ps1` emit, for
every row the clause reasons about, the untruncated path data instead of a capped
input summary: the full path for a file-reading row (a `use Read` row and a shell
row that reads a file), and the `Governing paths` and `Scope` path values for a
`use Agent` row. Rationale: the cap exists so an ordinary summary cannot flood
the reviewer's context, and paths are short and bounded in count, so emitting
them whole keeps the row small while removing the "open the truncated row"
workaround entirely. Whether the emitter also emits the matched set — the
intersection of a read path with a later brief's path lists — rather than only
the two path sides is left to the fix session, which should pick whichever keeps
the reference prose shortest while leaving the "could a worker have consumed it"
judgment in the lens; the emitter must not decide that judgment.

With the emitter carrying the paths, reduce the step 12 prose to naming the
emitter that produces the path evidence and the step 11 bounding mechanism the
finding must carry, and delete the normalization, membership-test, and
truncated-row-recovery mechanics from it. Update the script's own header rows
documentation, which is where the new row shapes belong.

The fix session must confirm from the current script that shell rows reading a
file are distinguishable from other shell rows before relying on that
distinction; if they are not, it should report that for re-planning instead of
inventing a classifier inside the emitter.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — the row emission and the header's documented row shapes, including the
  160-character cap declared at `:4`
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — step 12's
  isolation-report paragraph, the path-matching sentences only

## In scope
- In `Get-TranscriptProjection.ps1`: the `use` row emission for file-reading
  rows and `use Agent` rows, and the header comment block documenting the row
  shapes
- In `.agents/skills/next-plan-checkpoint-review/references/worker.md`: the
  path-matching sentences inside step 12's isolation-report paragraph, reduced
  to naming the emitter and the bounding mechanism

## Out of scope
- The isolation rule itself — what qualifies as isolation, its wording outside
  the path-matching sentences, step 13's exclusions, and step 11's precision
  guard
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
condition. Invariants: the projection still prints nothing for sidechain records
and still emits one row per tool call, tool result, main-assistant text element,
and string-content record; every non-path `use` row summary stays capped so no
row can flood the reviewer's context; the isolation lens still names an emitter
and a bounding mechanism for every finding, and its judgment of what qualifies as
isolation is unchanged.

## Acceptance criteria
- Running `Get-TranscriptProjection.ps1` against a transcript prints the full,
  untruncated path for each file-reading row and each `use Agent` row's
  `Governing paths` and `Scope` path values, with all other row shapes unchanged
- The script's header documents every emitted row shape it now produces
- Step 12's isolation-report paragraph contains no path normalization, membership
  test, or truncated-row recovery instruction, and names the emitter that
  supplies the path evidence
- Replaying the recorded pre-read symptom the emitter now exposes still yields a
  finding for a read a later brief lists, and none for a read no later brief in
  the run lists
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for the changed package

## Notes
The two neighbouring Plans both leave this script alone:
`Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md` and
`Documents/Plans/ChangeWorkflow/RunCheckpointTrims.md` each list
`Get-TranscriptProjection.ps1` under `## Out of scope`, so neither owns this root
cause and no `## Coordination` constraint is needed; a fix session should still
re-read step 12's current text, since the cited line numbers move.
