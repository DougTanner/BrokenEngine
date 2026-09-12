<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T18:54:03.234Z","dependsOn":[]} -->
# Fix: next-plan — the preparation handoff requires content no declared field carries and declares a field duplicating `Evidence`

## Context
Observed symptom. `.agents/skills/next-plan/SKILL.md` `## Handoff` (lines
171-183) bounds the preparation handoff to "every contradiction and unresolved
decision, the other verified Plan statements whose result requires a card or
implementation change, and one count of the unaffected statements", then
declares exactly three extension fields — `Claim`, `Classification`, and
`Execution card`. None of the three carries that bounded content, and
`Execution card` ("the file path plus its `##` selector") is the same value the
shared `Evidence` field already carries as path plus selector.

Rework it forced this run: the preparation worker returned the bounded content
as an undeclared prose block ("Verified statements needing a card change...",
"Unaffected statements checked and correct as written: 14.") and repeated the
`Evidence` row under `Execution card`. Both are forbidden by
`.agents/references/subagent-handoff.md` `## Handoffs`, which allows extension
only through declared fields, "never a paragraph", and states "Do not repeat a
row from another field". Main had to read the undeclared prose to get content
the skill itself demands, and reconcile the duplicated row.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 2369b425-8ca0-4359-91c6-899edbc93abf
- Worktree/branch UUID: e478531e-d807-4a5a-9aff-64168b704b02
- Session branch: claude/e478531e-d807-4a5a-9aff-64168b704b02
- Worktree: .claude\worktrees\BrokenEngine\e478531e-d807-4a5a-9aff-64168b704b02
- Landing ref: claude/e478531e-d807-4a5a-9aff-64168b704b02
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying the recorded client and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Recommended fix, because it is the smallest edit that closes both halves of the
symptom and leaves the shared form untouched: in `## Handoff`, add one declared
extension field — `Verification` is the recommended name, since the content is
the result of verifying the Plan's statements — whose one-line rows carry each
contradiction, each unresolved decision, each verified Plan statement whose
result requires a card or implementation change, and one final row giving the
count of unaffected statements; and keep `Execution card` while stating that the
card's path plus `##` selector is carried by `Execution card` alone and is never
repeated under the shared `Evidence` field. Keeping `Execution card` is
recommended over folding it into `Evidence` because main's presentation step and
the `### Execution card presentation/template` section below it both address the
card by that field name, so folding it would require edits there too.

Authorship note for the fix session: the recommendation above is this Plan
author's, with the rationale stated; the fix session may choose the alternative
of dropping `Execution card` and citing the card under `Evidence` instead, but
must then update every reference to the field name in the same section.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/next-plan/SKILL.md` — its bounding sentence, its declared
  extension-field list, and the `### Execution card presentation/template`
  wording only where the chosen field naming forces it

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared handoff form itself
- Every other section of `.agents/skills/next-plan/SKILL.md`, including
  `### Implementation approval` and the run steps
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. The shared form's rules stay authoritative: one
line per row, no paragraph, no repeated row, `Build required` present and
`Residuals` last. Never embed transcript paths or home paths.

## Acceptance criteria
- Every element the `## Handoff` bounding sentence requires is carried by a
  declared field, so a conforming preparation handoff needs no prose block
- No declared extension field restates a value another field of the same handoff
  carries
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
