<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T20:18:54.623Z","dependsOn":[]} -->
# Fix: progressive-disclosure-review SKILL.md — its handoff never says to omit passing checks, so returns carry assessment prose main does not need

## Context
`.agents/skills/progressive-disclosure-review/SKILL.md:45-70` defines the
returned handoff: three declared extension lines (`Skill:`, `Baseline:`,
`Files checked:`), the one-line `Findings` row form, and the statement that
`Changed files` and `Build required` are `none`. Nothing in it says what a
reviewer must leave out, so nothing rules out narrating the parts of the review
that found nothing.

`.agents/skills/coherence-review/SKILL.md:55-59` already carries that sentence
for the sibling instruction-prose review: "files read and checks that passed get
no block of their own: a check whose result decided something is a
`Decisive checks` row, and everything else is omitted."

In the observed run, a dispatched `/progressive-disclosure-review` returned one
finding plus a four-bullet assessment of the files it had cleared and a standing
note restating a fixed decision the dispatching brief had already given it. In
that run's `/next-plan` checkpoint review, this return and the one recorded in
`Documents/Plans/Engine/ValidateSkillHandoffDuplicateEvidence.md` were the only
two dispatch returns classed as carrying content the main session did not need.
No rework followed, so this is a bounding proposal, not a correctness defect.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 2f2a8c9f-a550-46b2-b743-8aea097e6123
- Worktree/branch UUID: ea52974a-a09b-4cbc-9c48-5c816a69ec1a
- Session branch: claude/ea52974a-a09b-4cbc-9c48-5c816a69ec1a
- Worktree: .claude\worktrees\BrokenEngine\ea52974a-a09b-4cbc-9c48-5c816a69ec1a
- Landing ref: claude/ea52974a-a09b-4cbc-9c48-5c816a69ec1a
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ea52974a-a09b-4cbc-9c48-5c816a69ec1a` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: add the omission sentence to
`progressive-disclosure-review/SKILL.md` `## Handoff`, worded from the
`coherence-review` precedent so the two instruction-prose reviews say the same
thing — a check whose result decided something is a `Decisive checks` row, files
read and checks that passed get no block of their own, and everything else is
omitted. Prefer referencing the precedent's wording over inventing a second
phrasing, so the two skills cannot drift. A brief-supplied fixed decision is
input, not a result, so the same sentence should make clear it is not restated
back.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/progressive-disclosure-review/SKILL.md` — `## Handoff`
  (`:45-70`)
- `.agents/skills/coherence-review/SKILL.md` — `## Handoff` (`:55-59`),
  read-only precedent for the omission wording
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:100-115`),
  read-only; owns the shared fields and their size limits

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `progressive-disclosure-review/SKILL.md`: adding the omission statement and,
  only if it would otherwise contradict it, the adjacent sentence naming what
  the declared extension lines carry

## Out of scope
- The review's judgment content: the duplication/misplacement/size classes, the
  size thresholds, and the `Findings` row form itself
- `## When to use`, `## Inputs`, and the trigger for dispatching this review
- `references/worker.md` steps, unless the added sentence directly contradicts
  one
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `.agents/skills/coherence-review/SKILL.md`, read-only precedent here
- `.agents/skills/validate-skill/SKILL.md`, owned by
  `Documents/Plans/Engine/ValidateSkillHandoffDuplicateEvidence.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: the three declared extension lines and the one-line `Findings` row
form stay unchanged, so a manager decides the same findings; `Changed files` and
`Build required` stay `none` for this findings-only review; `Residuals` stays
last; the focused re-review returns the same handoff. Never embed transcript
paths or home paths.

## Acceptance criteria
- `## Handoff`, read on its own, states that files read and checks that passed
  get no block of their own and that everything else is omitted
- The wording matches the `coherence-review` precedent rather than introducing a
  competing phrasing
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  file; plan validate exits 0
