<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T20:22:56.889Z","dependsOn":[]} -->
# Fix: /validate-skill — the `Accurate checks:` field returned nine confirmed-check bullets on a PASS result

## Context
A `/validate-skill` reviewer dispatched in this session returned a PASS result
whose `Accurate checks:` field held nine multi-sentence bullets, roughly 3,000
characters, restating every check that had confirmed nothing was wrong. The
manager used exactly one line of the whole handoff: the single Recommended
finding. Nothing else in the field changed a decision.

`.agents/skills/validate-skill/SKILL.md` `## Handoff` (`:30-49`) declares
`Accurate checks:` as `- confirmed check` with no bound on how many entries it
takes or how long one may be. The only limit that reaches it is the shared
10-row-per-field limit in `.agents/references/subagent-reporting.md`
`## Handoffs`, which nine bullets satisfy, so a reviewer that lists every
passing check is following the contract as written. `:49` sends only
per-package detail of a multi-package run to a `Temp/` file, so it does not
bound this field either.

`/coherence-review` already has the rule this field lacks. Its `Coherence`
field states at `.agents/skills/coherence-review/SKILL.md:57-59` that "files
read and checks that passed get no block of their own: a check whose result
decided something is a `Decisive checks` row, and everything else is omitted."

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 616368aa-4f28-451e-802e-f98a84986c30
- Worktree/branch UUID: 9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Session branch: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Worktree: .claude\worktrees\BrokenEngine\9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Landing ref: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: give `Accurate checks:` the rule `/coherence-review`
applies to its `Coherence` field — a check whose result decided something is a
`Decisive checks` row of the shared handoff, and everything else is omitted,
with a gitignored `Temp/` file cited under `Evidence` as the one place bulk
confirmations may travel when a later worker genuinely needs them — and say so
in the field's own
declaration so a reviewer reading only `## Handoff` cannot read the field as an
invitation to enumerate. Copy the shape of the existing rule rather than
inventing a new limit, and keep the field itself so a decisive confirmation
still has somewhere to go. Confirm while doing so whether
`.agents/skills/validate-skill/references/worker.md` restates the field, and
keep the two consistent without duplicating the rule.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/validate-skill/SKILL.md` — `## Handoff` (`:30-49`), the
  `Accurate checks:` declaration
- `.agents/skills/validate-skill/references/worker.md` — checked for a
  restatement of the same field
- `.agents/skills/coherence-review/SKILL.md` — read-only precedent for the
  decided-something rule

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Accurate checks:` declaration in
  `validate-skill/SKILL.md` `## Handoff` and any matching restatement in that
  skill's `references/worker.md`

## Out of scope
- The Status values, the Critical/Recommended finding split, and the mechanical
  runs those fields report
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `coherence-review`, which is read-only precedent here
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: `Status`, `Mechanical evidence:`, `Critical findings:` and
`Recommended findings:` keep their current meanings and are still returned; a
PASS still requires both mechanical runs to have succeeded; the shared
handoff's required lines are all still returned, with `Residuals` last. Never
embed transcript paths or home paths.

## Acceptance criteria
- The `Accurate checks:` declaration, read on its own, admits only a check
  whose result decided something, and names where other confirmations travel
- A validation run that confirms many checks and finds nothing returns a
  handoff whose length is set by its findings, not by its check count
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
