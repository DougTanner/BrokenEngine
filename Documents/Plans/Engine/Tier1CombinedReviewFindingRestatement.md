<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T00:12:10.034Z","dependsOn":[]} -->
# Fix: Tier-1 combined review pass — its `Coherence` section restates findings the shared handoff already carries

## Context
During a `/next-plan` run, the Tier-1 combined review pass dispatched per
`.agents/references/tier1-combined-review.md` reviewed
`.agents/references/subagent-reporting.md` and returned each coherence finding
twice: findings C1 and C2 appeared as shared-handoff `Findings` rows and were
then restated in full prose under the pass's own `## Coherence` section. The same
handoff also carried two blocks belonging to no field the contract defines — an
"Accuracy checks that passed" list repeating what `Decisive checks` already
stated, and a six-path "Files read:" list. The checkpoint reviewer recorded this
as class `fixable-defect` under the isolation lens.

The emitter is `.agents/references/tier1-combined-review.md` `## Result sections`
(`:38-48`). It defines a `Coherence` section as "findings with file:line and
severity, or an explicit no-finding statement", which is the same content the
shared handoff's `Findings` field already requires as one row each
(`.agents/references/subagent-reporting.md` `## Handoffs`), and that same section
says "do not repeat a row from another field". The contract also never states
where files read or passed checks belong, so a reviewer adding them is not
contradicting anything written. The result is that every Tier-1 combined pass
pays for each coherence finding twice in main's context, plus whatever unlisted
extras the reviewer chooses to append.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: cee154a8-ef27-4ba3-aa4d-921bb4b4d75d
- Worktree/branch UUID: d25f3fee-bca9-498a-8d55-f52544dd1699
- Session branch: claude/d25f3fee-bca9-498a-8d55-f52544dd1699
- Worktree: .claude\worktrees\BrokenEngine\d25f3fee-bca9-498a-8d55-f52544dd1699
- Landing ref: claude/d25f3fee-bca9-498a-8d55-f52544dd1699
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/Tier1CombinedReviewFindingRestatement.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`,
which already cites the emitting section and the shared-handoff rule it collides
with. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/d25f3fee-bca9-498a-8d55-f52544dd1699` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID. Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommendation, which the implementing session confirms or replaces
from the cited files, is the bounding the checkpoint reviewer proposed: state in
`## Result sections` that the `Coherence` section references each shared
`Findings` row by its ID and adds only what a one-line row cannot carry, and
state that files read and checks that passed belong under `Decisive checks` or
nowhere. The rationale is that it removes the restatement without inventing a new
mechanism and without changing what main can read.

Two things must be settled before writing that prose, because they decide whether
the recommendation survives:

1. Whether a `Coherence` section reduced to ID references still satisfies the
   Step-5 coherence pass condition in the same reference (`:16-18`), which turns
   on accepted semantic findings being resolvable — read that section and the
   shared `Findings` row shape together before trimming.
2. Whether anything downstream reads the `Coherence` prose rather than the
   `Findings` rows. If a consumer needs the prose, the alternative fix is the
   mirror image: keep the prose and say the combined pass omits those findings
   from the shared `Findings` field. Prefer whichever leaves main reading the
   same evidence it reads today with fewer bytes.

Verify every cited line number against the current tree before relying on it.

## Critical files
- `.agents/references/tier1-combined-review.md` — `## Result sections`
  (`:38-48`), the section that defines `Coherence` and `Criteria`
- `.agents/references/subagent-reporting.md` — `## Handoffs`, read for the
  `Findings` row shape and the no-repeated-row rule; edited only if the chosen
  resolution provably requires it

## In scope
- Root-cause investigation as `## Design` states, including settling the two
  questions it names
- The smallest resulting fix, confined to the files named above — within them,
  the `Coherence` and `Criteria` bullets of `## Result sections` in
  `.agents/references/tier1-combined-review.md`, and, only if provably required,
  the `Findings` row description in `## Handoffs` of
  `.agents/references/subagent-reporting.md`

## Out of scope
- Which components the combined pass carries, when it applies, its routing, and
  its fix-round rule
- The pass conditions of components 1, 2, and 3, except as read to settle
  question 1 above
- The verbatim Step-6 sub-handoff requirement and its size, which
  `Documents/Plans/Engine/Tier1CombinedReviewHandoffSize.md` owns
- The shared handoff field list, the `Executor` rules, and the size cap
- Any skill body
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (reference prose only at its existing layer); escalate to Tier 2
if the chosen fix changes a pass condition or what the landing acceptance table
scores, because that is that gate's scoped behavior. Every accepted semantic
finding must remain individually identifiable and resolvable after the trim.
Never embed transcript paths or home paths in tracked files.

## Acceptance criteria
- A Tier-1 combined pass with coherence findings has exactly one documented place
  each finding is stated in full, with the other place referencing it by ID
- The contract states where files read and checks that passed belong, so a
  reviewer adding an unlisted block is contradicting written guidance
- /progressive-disclosure-review passes for the changed instruction prose; plan
  validate exits 0

## Notes
No dependency or Coordination edge is recommended: the fix is independently
landable and must not depend on the Plan the observing session was completing.
`Documents/Plans/Engine/Tier1CombinedReviewHandoffSize.md` edits the same
`## Result sections` region for a different symptom — the verbatim Step-6 blocks
exceeding the shared size cap — so whichever lands second reconciles its wording
against the other's landed text; neither blocks the other and either order works.
