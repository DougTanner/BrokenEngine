<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T20:18:48.430Z","dependsOn":[]} -->
# Fix: validate-skill SKILL.md — its handoff returns the mechanical run rows twice and lets `Accurate checks` grow unbounded

## Context
`.agents/skills/validate-skill/SKILL.md:30-47` declares the extension field
`Mechanical evidence:` on the form `command, exit, decisive output` (`:37-38`).
Those are the same three facts the shared handoff's `Decisive checks` field
already carries — `.agents/references/subagent-reporting.md:104` defines that
field as "one row each, command or read and its result" — so a reviewer filling
both fields returns each mechanical run twice, against
`.agents/references/subagent-reporting.md:111`: "Do not quote code and do not
repeat a row from another field."

The same section's `Accurate checks:` field (`:45-46`) says only "confirmed
check", with `references/worker.md:38` adding "List confirmed passes under
`Accurate checks`". Neither states a cap or a form. In the observed run a
dispatched `/validate-skill` reviewer returned ten multi-sentence bullets there,
one per contract it had confirmed. In that run's `/next-plan` checkpoint review,
this return and the one recorded in
`Documents/Plans/Engine/ProgressiveDisclosureHandoffOmissionRule.md` were the
only two dispatch returns classed as carrying content the main session did not
need. No rework followed, so this is a bounding proposal, not a correctness
defect.

`.agents/skills/coherence-review/SKILL.md:55-59` is the working precedent for
the opposite treatment: a check whose result decided something is a
`Decisive checks` row, and everything else is omitted.

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

The author's recommendation, in two parts:
- Delete the `Mechanical evidence:` extension field and let the two mechanical
  runs travel as ordinary `Decisive checks` rows, which is where the shared form
  already puts a command with its exit and result. If any fact that field
  carried has no home in `Decisive checks`, keep exactly that fact and say what
  makes it different.
- Bound `Accurate checks:` to a count of contracts that passed plus a row per
  deviation, and cite the validated package path plus the section selectors
  covering the rest, the way the shared form's `Evidence` field already
  transports material a row cannot hold.

Both edits are wording inside `## Handoff`, with a matching one-line correction
to `references/worker.md:38` so the worker's step 7 and the public form agree.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/validate-skill/SKILL.md` — `## Handoff` (`:30-47`)
- `.agents/skills/validate-skill/references/worker.md` — step 7 (`:38-39`)
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:100-115`),
  read-only; owns the shared fields and the no-repeated-row rule
- `.agents/skills/coherence-review/SKILL.md` — `## Handoff` (`:55-59`),
  read-only precedent for the omission wording

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `validate-skill/SKILL.md`: the `Mechanical evidence` field, the
  `Accurate checks` field and its size bound, and the sentence that decides
  where overflow prose travels
- The single matching sentence in `validate-skill/references/worker.md` step 7,
  changed only so it does not contradict the revised public form

## Out of scope
- The validation judgment itself: which mechanical and semantic contracts are
  checked, the `Status` values, and the Critical/Recommended split
- `## When to use`, `## Inputs`, and the trigger for dispatching this review
- The bundled scripts under `.agents/skills/validate-skill/scripts/`
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `.agents/skills/coherence-review/SKILL.md`, read-only precedent here
- `.agents/skills/progressive-disclosure-review/SKILL.md`, owned by
  `Documents/Plans/Engine/ProgressiveDisclosureHandoffOmissionRule.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: the `Status` value set and the Critical/Recommended meanings stay
unchanged, so a manager decides the same results; every required shared-handoff
line is still returned, with `Build required` present and `Residuals` last; a
`BLOCKED` result still names its missing input. Never embed transcript paths or
home paths.

## Acceptance criteria
- Reading `## Handoff` alone, each mechanical run has exactly one home, and no
  declared field asks for a row another field already carries
- `Accurate checks` has a stated bound and a stated place for the material the
  bound excludes, with a citation form for it
- `references/worker.md` step 7 states the same thing as the revised public form
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
