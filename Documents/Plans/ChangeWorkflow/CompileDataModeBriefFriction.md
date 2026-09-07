<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T14:43:30.766Z","dependsOn":[]} -->
# Fix: /compile — builder dispatch blocks on Local data-mode authorization the brief could not have known to include

## Context
Observed symptom during a `/next-plan` run's targeted pre-review checks step. A
`builder` dispatched for `/compile` on a change whose only edits were whitespace
in DataPacker source returned BLOCKED, reporting that Local is mandatory and that
8 changed `DataPacker/**` files trigger this rule unconditionally by path. Main
then obtained the Local generation authorization and re-dispatched the same
build — one wasted builder dispatch plus one authorization round-trip.

Emitter evidence in the current tree:
- `.agents/skills/compile/SKILL.md`, `## Inputs` (the bullet at about line 43),
  has the caller supply "any Local generation or Gaea authorization a
  user-approved plan or acceptance criterion grants", and points at
  `references/runtime-data-mode.md` for which authorizations are valid.
- Nothing in the public files tells the dispatcher to resolve the data mode before
  writing the brief. The read-only resolver
  `.agents/skills/compile/scripts/Resolve-CompileContext.ps1` is named only in the
  private `.agents/skills/compile/references/worker.md` (line 15), which a
  dispatching manager does not read, so a manager cannot learn from the public
  files that a `DataPacker/**` change will need authorization until the worker
  blocks.
- `.agents/skills/compile/references/runtime-data-mode.md` line 8 makes Local
  mandatory whenever changed paths touch `DataPacker/**`, with no statement about
  proven behavior-preserving DataPacker source changes; line 9's exemption covers
  only `AGENTS.md`/`CLAUDE.md` filenames, and line 12's exception covers only pure
  asset deletions. That rule is not a defect and is not changed by this Plan; it
  is recorded here only because it is what the blocked dispatch reported.

Cost: one blocked builder dispatch plus one user or manager authorization
round-trip, on every change touching `DataPacker/**`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: 223c2800-572a-4c1d-9723-ba093e3ccb2c
- Worktree/branch UUID: 32ceb1c0-59a8-473b-a35e-db107b897125
- Session branch: claude/32ceb1c0-59a8-473b-a35e-db107b897125
- Worktree: .claude\worktrees\BrokenEngine\32ceb1c0-59a8-473b-a35e-db107b897125
- Landing ref: claude/32ceb1c0-59a8-473b-a35e-db107b897125
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the recorded
client and conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below.

The fix is dispatcher-side only: make `.agents/skills/compile/SKILL.md`
`## Inputs` state that the dispatcher resolves the data mode before writing the
brief, by running the read-only resolver the skill owns, and then carries the
resolved mode in the brief together with either the Local generation
authorization or an explicit statement of the basis for Shared. That removes the
blocked dispatch without touching the mode rule itself.

The Local-mode trigger rule at `references/runtime-data-mode.md` line 8 stays
unchanged; this Plan adds no carve-out for behavior-preserving DataPacker source
changes. A whitespace-only edit is cheap to authorize once the dispatcher knows
to ask, and any such carve-out would need a proof rule that the deletion-only
exception at line 12 shows is costly to state correctly.

If root-causing shows the fix lies outside the boundary below — for example that
`Resolve-CompileContext.ps1` must change — surface it for re-planning instead of
expanding scope.

## Critical files
- `.agents/skills/compile/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to the `## Inputs` section of
  `.agents/skills/compile/SKILL.md`

## Out of scope
- `.agents/skills/compile/references/runtime-data-mode.md`, including its
  Local-mode trigger rule
- `.agents/skills/compile/scripts/**` behavior, parameters, and output schemas
- `.agents/skills/compile/references/worker.md` and the worker's build execution
  contract
- Gaea and expensive-export authorization rules
- The landed change the observing session produced
- Unrelated skills and scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation prose that changes no runtime or tool behavior);
the Local-mode trigger rule is out of scope, so no data-regeneration behavior
changes. Escalate to Tier 3 if root-causing shows the fix must reach the
build/bootstrap coordination the resolver scripts perform. Never embed transcript
paths or home paths.

## Acceptance criteria
- A manager composing a `/compile` brief from the public files alone is told to
  resolve the data mode first and to carry the resolved mode plus either the
  Local authorization or the stated Shared basis in the brief, so the recorded
  BLOCKED symptom no longer reproduces for a `DataPacker/**` change
- `.agents/skills/compile/references/runtime-data-mode.md` is unchanged by the
  fix
- `/validate-skill` passes wherever the root AGENTS.md Apply the triggered cleanup
  step triggers it; plan validate exits 0

## Notes
Recorded as tooling friction at a `/next-plan` claim exit; the observed symptom
substitutes for a confirmed root cause, which the fix session establishes.
