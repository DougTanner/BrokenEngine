<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T19:23:15.377Z","dependsOn":[]} -->
# Fix: /next-plan-checkpoint-review — the isolation lens misses main pre-reading a later dispatch's governing paths

## Context
Observed symptom, recorded by a `/next-plan-review` of an earlier session. In
that session main read `.agents/references/subagent-reporting.md` (8,282
characters) and `.agents/references/subagent-handoff.md` (3,188 characters) at
18:24:47Z-18:24:53Z, then eleven seconds later dispatched a preparation
`implementer` whose brief listed both files under `Governing paths` and whose
acceptance required verifying those exact citations. That is the content the
dispatched worker was about to consume itself, against
`.agents/skills/next-plan/SKILL.md:71-74`, which has main leave reading the
target source to the dispatched `implementer`.

The run checkpoint reviewed that same run. Its isolation lens
(`.agents/skills/next-plan-checkpoint-review/references/worker.md`, the
isolation-report paragraph at `:114-121` inside step 12, its exclusions at
step 13, `:122-126`, and the precision guard at step 11) flagged only a
1,830-character
`measurement.md` read and reported zero rows over threshold, so the 11,470
characters of pre-read governing paths went unreported until the later
`/next-plan-review`.

Step 12 already reports as isolation "main reading a source or reference file a
worker's brief could have named", and already states "Size does not gate this",
so both reads fall inside the rule as written. The recorded miss is therefore
not an absent rule but an unapplied one: nothing in the lens tells the reviewer
how to detect that case, so the rule did not fire on it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e6a8bdcc-562d-4d35-9ed0-416a914b687d
- Worktree/branch UUID: cf9c7025-5876-47d6-97be-085f5dc41b2d
- Session branch: claude/cf9c7025-5876-47d6-97be-085f5dc41b2d
- Worktree: .claude/worktrees/BrokenEngine/cf9c7025-5876-47d6-97be-085f5dc41b2d
- Observed in an earlier session: the fields above are the observing session's,
  not the recording session's; that session landed commit
  a4d29b504622c2f2a567140ef6fe537f8024fdff.
- Landing ref: claude/637a7f16-f462-48b6-9ea7-79aa886b3d95, the recording
  session's branch.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CheckpointIsolationGoverningPathPreRead.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause why step 12's existing rule did not fire on the two reads —
whether the cause is the rule's wording, the evidence the lens is told to read,
or the span of transcript it examines. Read the isolation lens and its precision
guard in `.agents/skills/next-plan-checkpoint-review/references/worker.md`, and
the projection row shapes documented in the header of
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`,
and establish whether the projection already exposes both a main-session file
read and a later delegation event's brief text within one run — the fix depends
on that. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review a4d29b504622c2f2a567140ef6fe537f8024fdff` in bounded friction
mode, supplying the recorded client `claude` and the recorded conversation
session ID above.

Then make the smallest fix inside the `## In scope` boundary below. The author's
recommendation is to make step 12's existing rule detectable rather than to add
a second owning rule for the same content: give it the check that matches the
paths main read in the run against the `Governing paths` and `Scope` of later
dispatch briefs in that same run, with the dispatched role named as the bounding
mechanism the precision guard already demands. Rationale: the later brief is the
proof that a worker was going to consume the content, which is exactly the
"a worker's brief could have named" condition step 12 already states, so the
check adds no threshold and cannot fire on a read main had to make for its own
decision. If the projection does not expose brief text, the fix session surfaces
that for re-planning rather than extending the script, because the script's row
shapes are out of scope here.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — the
  isolation-report paragraph at `:114-121` inside step 12, its exclusions at
  step 13, and the precision guard at step 11

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the isolation-report paragraph at
  `:114-121` inside step 12, step 13 (`:122-126`), and the precision guard at
  step 11 in `.agents/skills/next-plan-checkpoint-review/references/worker.md`

## Out of scope
- The landed change the observing session produced
- The friction and context lenses, and the checkpoint's dispatch condition
- `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`
  and `Get-TranscriptProjection.ps1`
- `/next-plan-review` and its measurement rules
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior — one skill package's review rules);
escalate if the fix reaches the bundled scripts or the checkpoint's dispatch
condition. Invariants: the precision guard still requires a named emitter and a
named bounding mechanism for every isolation finding; the lens still produces no
finding on a run with no implementation content in span; no transcript path or
home path is embedded.

## Acceptance criteria
- The existing rule in the isolation-report paragraph at `worker.md:114-121`
  inside step 12 carries the governing-path check exactly once, no second owning
  isolation rule is added for the same content, and the check names the bounding
  mechanism the precision guard requires
- Replaying the recorded symptom against the rule after the fix yields a finding for
  the two pre-read reference files and no finding for a read whose path no later
  dispatch in the run lists
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for the changed package
