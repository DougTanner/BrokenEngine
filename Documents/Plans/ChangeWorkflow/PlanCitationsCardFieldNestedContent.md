<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T19:56:08.528Z","dependsOn":[]} -->
# Fix: Test-PlanCitations.ps1 — card fields with nested-bullet content or a parenthetical label reported missing

## Context
Observed symptom: in a `/next-plan` preparation, running
`pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 Temp/FleetMemberGlobalIdReferences-prep.md`
reported `card.missingFields` = `interfacesAndInvariants`, `acceptanceChecks`,
`roles` although the execution card carried all three fields. The card wrote
`- Interfaces and invariants:` and `- Acceptance checks:` with their content
only as nested bullets on the following lines, and `- Roles (from ...):` with a
parenthetical between the label and the colon.

The bullet-field patterns in `$script:CardFields`
(`.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1:34-36`, e.g.
`'^-\s+Interfaces and invariants:\s*\S'`, `'^-\s+Roles:\s*\S'`) require text
after the colon on the bullet line itself, and the label to be followed
directly by the colon, so neither shape matches. The template in
`.agents/skills/next-plan/SKILL.md` `### Execution card presentation/template`
shows each field on one line (`- Interfaces and invariants: <contracts>`) but
does not state that nested content or a qualified label is disallowed.

Rework it forced: the preparation worker had to mark the card result advisory,
so the script never actually checked card completeness for that Plan; then
`/plan-audit` raised finding PA-F-004 asking to reformat the card only to
satisfy the script.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 0416c57a-19d5-4ce3-a276-9ee57cf6441c
- Worktree/branch UUID: aac539bc-6123-4b3c-a603-da8e0253b372
- Session branch: claude/aac539bc-6123-4b3c-a603-da8e0253b372
- Worktree: .claude/worktrees/BrokenEngine/aac539bc-6123-4b3c-a603-da8e0253b372
- Landing ref: claude/aac539bc-6123-4b3c-a603-da8e0253b372
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/PlanCitationsCardFieldNestedContent.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The fix direction is for the implementing session to choose; the author
recommends choosing one of these and keeping the script and the template in
agreement:
- make the bullet-field detection accept content carried by nested bullets
  under the field line (and, if wanted, a qualified label before the colon), or
- keep the one-line form and state it as the required card shape in the
  template, so preparation writes cards the script can check.
The author leans toward the first, since nested bullets are a natural shape for
multi-item invariants and checks, but either removes the false report.

## Critical files
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1`
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `$script:CardFields` patterns and
  the card-field matching function in `Test-PlanCitations.ps1`, and/or the
  `### Execution card presentation/template` section of
  `.agents/skills/next-plan/SKILL.md`

## Out of scope
- The landed change the session produced
- The citation and heading lookups in `Test-PlanCitations.ps1`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths. The
script stays read-only and keeps its `broken-engine-plan-citations/v1` result
shape.

## Acceptance criteria
- A card whose `Interfaces and invariants`, `Acceptance checks`, or `Roles`
  field is written in the shape the template permits after the fix is not
  reported in `card.missingFields`, and a card missing one of those fields is
  still reported
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
