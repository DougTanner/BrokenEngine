<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-13T00:35:24.175Z","dependsOn":[]} -->
# Fix: next-plan — step 4 bounds the worker's snapshot citation but not main's read of it

## Context
`.agents/skills/next-plan/SKILL.md` step 4 (lines 87-106) has the preparation
`implementer` write the complete resolved Plan and execution card into one
gitignored `Temp/` file and "cite that path plus `## Execution card` selector
under `Evidence`", and its Done condition requires that the handoff "cites it as
one file path plus `##` selector". Both requirements bind the worker's citation
only. Nothing in that step, and nothing in the `Evidence` row rule main follows
in `.agents/references/subagent-handoff.md` lines 27-31 — which states only that
the cited file "is for the workers main dispatches next, cited to them as path
plus selector" — bounds what main itself then reads from that snapshot.

Observed symptom in this session: after the preparation handoff cited two
selectors under `Evidence`, and after main's own grep over the snapshot had
already returned its `##` heading offsets, main read the whole preparation
snapshot rather than the cited selectors. The `## Resolved mechanism` half read
outside the citations was discarded when the alternatives step redrafted the
snapshot shortly afterwards, so those bytes bought nothing.

Context-efficiency envelope for the observed cost, recorded at the `/next-plan`
run checkpoint:
- Tool: Read
- Invocation: whole-file read of the preparation snapshot
  `Temp/AgentActivateFrameFixture-prepared.md`, after a grep of the same file
  had returned its `##` heading offsets and the handoff had cited two selectors
  under `Evidence`
- Measured size: about 21,000 characters, above the 20,000-character
  per-result threshold; the half outside the cited selectors was superseded by
  the alternatives step's redraft of the same snapshot
- Checkpoint: `/next-plan` run checkpoint of the recording session

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: c7ef7a0d-756e-45f2-b7dc-6170fd8b2573
- Worktree/branch UUID: dfb7fc91-292f-43d9-a66a-248f23eed1d0
- Session branch: claude/dfb7fc91-292f-43d9-a66a-248f23eed1d0
- Worktree: .claude\worktrees\BrokenEngine\dfb7fc91-292f-43d9-a66a-248f23eed1d0
- Landing ref: the session branch above. The recording session had not landed
  its change when this Plan was written, so the branch tip named here contains
  this Plan only once that session's work is committed to it; until then the
  Plan exists as tracked content in the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines should be sufficient, so the transcript is not expected to be
needed. Only when it genuinely is, in a new session run
`/next-plan-review <landing ref above>` in bounded friction mode, supplying
client `claude` and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is to state the reader-side bound in
`.agents/skills/next-plan/SKILL.md` step 4, next to the existing citation
requirement: main reads a cited preparation snapshot only at the selectors the
handoff cites, addressing them by the `##` heading offsets it already has, never
whole. Keeping the rule in step 4 confines it to the one snapshot whose observed
cost this Plan records and leaves the general `Evidence` row rule in
`.agents/references/subagent-handoff.md` untouched; an implementer who finds
during root-causing that the same unbounded read reaches every cited `Evidence`
file, not just this snapshot, may instead place the bound in that shared rule
and have step 4 rely on it, which avoids stating the same rule twice. The fix
must not weaken step 4's existing citation requirement, its `Test-PlanCitations.ps1`
`Decisive checks` row, or the Done condition.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — step 4
- `.agents/references/subagent-handoff.md` — the `Evidence` row rule, only if
  the shared-rule route is taken

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to step 4 of
  `.agents/skills/next-plan/SKILL.md`, plus the `Evidence` row rule paragraph of
  `.agents/references/subagent-handoff.md` only if the shared-rule route is
  taken

## Out of scope
- The landed change the session produced
- What the preparation `implementer` writes into the snapshot, and the snapshot's
  own structure
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` and its result shape
- The alternatives step and its redraft of the snapshot
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation-only skill prose with no behavior or invariant exposure);
escalate to Tier 2 if the fix reaches the shared handoff form's structure or a
script. Main must still obtain everything the approval presentation and the Plan
review dispatches require. Never embed transcript paths or home paths.

## Acceptance criteria
- Step 4 states that main reads a cited preparation snapshot only at the cited
  selectors, by heading offset, and never whole
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
