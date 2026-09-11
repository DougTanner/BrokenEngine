<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T22:28:01.668Z","dependsOn":[]} -->
# Fix: compile — `## Inputs` sends the dispatcher through a whole reference for a two-section decision

## Context
`.agents/skills/compile/SKILL.md` `## Inputs` links
`references/runtime-data-mode.md` whole, twice, for the dispatcher's data-mode
decision. That reference is about 9,700 characters across seven sections, but
only `## Mode selection` and `## Local generation` settle what the dispatcher
must decide: which mode applies and whether the brief carries the Local
generation authorization. The remaining four sections —
`## Repository-root detection and mode derivation`,
`## Build invocation and derived properties`,
`## Generated Data directory inventory`, and
`## Wrapper bootstrap Shared-data refresh` — are builder mechanics the worker
reads itself, as `.agents/skills/compile/references/worker.md:89` requires
("Done when `runtime-data-mode.md` has been read").

Context-efficiency envelope for the observed cost, recorded at the `/next-plan`
run checkpoint:
- Tool: Read
- Invocation: read of
  `.agents/skills/compile/references/runtime-data-mode.md` in full, as
  `## Inputs` directs the dispatcher
- Measured size: about 9,700 characters, of which the two decisive sections are
  a minority; the balance is worker-owned mechanics re-read by the worker
- Checkpoint: `/next-plan` run checkpoint of the recording session

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1e48cbdc-67da-4277-bcf1-3548f3ae04e4
- Worktree/branch UUID: a75970c8-ed2b-4c66-a830-960f0204d3ac
- Session branch: claude/a75970c8-ed2b-4c66-a830-960f0204d3ac
- Worktree: .claude\worktrees\BrokenEngine\a75970c8-ed2b-4c66-a830-960f0204d3ac
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
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID above. Then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is to keep both existing links but bound the
dispatcher's read to the two decisive headings — citing `## Mode selection` and
`## Local generation` as the sections the dispatcher reads — so the worker-owned
mechanics stay out of the dispatching context while the reference remains the
single owner of every data-mode rule. No content moves out of
`references/runtime-data-mode.md`, and the worker's own whole-reference read
stays as `worker.md` states it.

## Critical files
- `.agents/skills/compile/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Inputs` section of
  `.agents/skills/compile/SKILL.md`

## Out of scope
- `.agents/skills/compile/references/runtime-data-mode.md` content and section
  structure
- `.agents/skills/compile/references/worker.md` and its whole-reference read
- The `references/prefast-mode.md` link and the resolver invocation prose,
  except where the data-mode citation is worded
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation-only skill prose with no behavior or invariant exposure);
escalate only if the fix moves data-mode rules between files. The Local
generation authorization requirement must remain stated for the dispatcher, and
`references/runtime-data-mode.md` must remain the single owner of the data-mode
rules. Never embed transcript paths or home paths.

## Acceptance criteria
- `## Inputs` cites `## Mode selection` and `## Local generation` as the
  dispatcher's bounded read, and still requires the Local generation
  authorization in the brief
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
