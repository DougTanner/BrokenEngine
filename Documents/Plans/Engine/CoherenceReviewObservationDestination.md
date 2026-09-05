<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:21:26.845Z","dependsOn":[]} -->
# Fix: coherence-review SKILL.md — a PASS run appended a pre-existing observation as a paragraph, because no destination is named for one

## Context
A `/coherence-review` reviewer dispatched in this session returned a PASS
handoff with `Findings: none`, and then appended a paragraph after the fenced
handoff, labelled "Non-blocking observation, offered only as context for main
(not a finding, pre-existing and unchanged by this diff)". The manager could act
on none of it: the reviewer had already stated it was not a finding and not
caused by the diff, so nothing about it was decidable.

`.agents/skills/coherence-review/SKILL.md:55-59` defines the `Coherence`
extension field as one line per semantic finding or an explicit no-finding
statement, and adds that "files read and checks that passed get no block of
their own: a check whose result decided something is a `Decisive checks` row,
and everything else is omitted." That sentence covers a check that passed. It
does not cover an observation that is neither a finding nor a check result — an
unchanged pre-existing condition the reviewer noticed — so the reviewer had no
named destination for it and attached it as free prose outside the handoff.

The shared handoff at `.agents/references/subagent-reporting.md:95-98` already
requires a handoff to return only decision-relevant evidence, and at `:134-137`
allows a skill to extend the form only by rows inside an existing field or by
declared extension fields, "each one line or one row per item, never a
paragraph". The appended paragraph is outside both.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8ef0714e-d5fa-48b2-a4a8-00788620d328
- Worktree/branch UUID: c5715c35-1334-459b-a278-e8aa1ae47572
- Session branch: claude/c5715c35-1334-459b-a278-e8aa1ae47572
- Worktree: .claude\worktrees\BrokenEngine\c5715c35-1334-459b-a278-e8aa1ae47572
- Landing ref: claude/c5715c35-1334-459b-a278-e8aa1ae47572
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/c5715c35-1334-459b-a278-e8aa1ae47572` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: extend the `Coherence` field's existing omission
sentence so it also names where a pre-existing observation the diff did not
change may go — either one `Residuals` row on a single line, or omitted
entirely, with any supporting prose written to a gitignored `Temp/` file cited
under `Evidence` as path plus selector. `.agents/skills/plan-audit/SKILL.md:69-71`
is the recorded precedent for that direction: "statements the audit checked and
cleared stay in the auditor's own context and travel only as a gitignored
`Temp/` file cited under `Evidence`, never as inline prose." Prefer reusing that
wording over inventing a third phrasing, so the review skills cannot drift. The
fix session decides whether `Residuals` or omission is the right destination and
whether one sentence or two is the smallest change; this Plan does not decide it
beyond the `## In scope` boundary below.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/coherence-review/SKILL.md` — `## Handoff` (`:49-62`), the
  `Coherence` field and its omission sentence
- `.agents/skills/plan-audit/SKILL.md` — `:69-71`, read-only precedent for the
  destination wording
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:95-137`),
  read-only; owns the shared fields, the never-a-paragraph rule, and the
  overflow route

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Coherence` extension field's
  destination wording in `## Handoff` of `coherence-review/SKILL.md`

## Out of scope
- The review's judgment content: what semantic coherence means, the checks the
  review runs, and the finding row form itself
- The `Apply the triggered cleanup` and `Criteria` extension fields, the Tier-1
  combined-pass mode, and the verbatim cleanup-handoff rule
- `## When to use`, `## Inputs`, and the trigger for dispatching this review
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `.agents/skills/plan-audit/SKILL.md`, read-only precedent here
- `.agents/references/scope-authorization.md`, whose passing-run reporting is
  owned by `Documents/Plans/Engine/ScopeAuthorizationPassReport.md`
- The change this session landed in
  `.agents/skills/plan-simplicity-review/SKILL.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: the `Coherence` field still carries one line per semantic finding or
an explicit no-finding statement; the existing passed-check omission sentence
keeps its meaning; the shared handoff's required lines are all still returned,
with `Build required` present and `Residuals` last; Tier-1 combined-pass
behavior is unchanged. Never embed transcript paths or home paths.

## Acceptance criteria
- `## Handoff`, read on its own, names exactly one destination for an unchanged
  pre-existing observation, and rules out attaching it as prose beside the
  handoff
- A reviewer following it on a change with no finding returns the handoff lines
  and nothing else
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  file; plan validate exits 0

## Notes
`Documents/Plans/Engine/ScopeAuthorizationPassReport.md` records a different
symptom from the same review skill's returns — a narrated scope-authorization
mapping — but its fix boundary is the shared reference
`.agents/references/scope-authorization.md`, not this skill file, so the two do
not overlap.
