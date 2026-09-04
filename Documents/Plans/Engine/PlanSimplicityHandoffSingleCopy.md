<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:00:17.552Z","dependsOn":[]} -->
# Fix: plan-simplicity-review SKILL.md — its handoff form makes the reviewer list every finding twice

## Context
`.agents/skills/plan-simplicity-review/SKILL.md:71-92` gives the finding form as
a multi-clause block — `PSR-F-###`, severity, plan location, class, problem,
evidence, occurrence/likelihood, simpler alternatives, cost comparison,
disposition — and then says at `:92` "Those entries are the rows of the shared
handoff's `Findings` field." A dispatched reviewer read that as list the entries,
then list them again as `Findings` rows, and returned each entry twice: once in
full and once as a row. The returned handoff measured about 9,539 characters,
against the 20,000-character whole-handoff cap in
`.agents/references/subagent-reporting.md` `## Handoffs`, and the manager read
every finding twice.

`.agents/skills/plan-audit/SKILL.md:56-58` uses the same sentence without the
problem, because its finding form is already one line; the multi-clause form here
is what makes the same sentence ambiguous and what pushes a row past one line.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 62f98e1f-3ac2-49b9-8c3a-08d631bb56db
- Worktree/branch UUID: ec8cd668-9c77-4241-bf54-16269ae03fa8
- Session branch: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Worktree: .claude\worktrees\BrokenEngine\ec8cd668-9c77-4241-bf54-16269ae03fa8
- Landing ref: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ec8cd668-9c77-4241-bf54-16269ae03fa8` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: rewrite `## Handoff` so each finding is returned
exactly once, as one `Findings` row on a single line, and say so in words that
cannot be read as a second listing — the `/plan-audit` `## Handoff` wording is
the working single-copy precedent to follow. Keep in the row only what the
manager needs to decide the finding — ID, severity, plan location, class,
problem, evidence locator, disposition — and send the longer
occurrence/likelihood, simpler-alternative, and cost-comparison prose to a
gitignored `Temp/` file cited under `Evidence` as path plus selector, which the
skill already permits at `:100-103` for a clean result's judgment notes.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-simplicity-review/SKILL.md` — `## Handoff` (`:70-110`)
- `.agents/skills/plan-audit/SKILL.md` — `## Handoff` (`:52-73`), read-only
  precedent for the single-copy wording

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `plan-simplicity-review/SKILL.md`: the finding form, the sentence at `:92`,
  the `Steps reviewed:` extension field's placement, and where the longer prose
  travels

## Out of scope
- The review's judgment content: the finding classes, the Q1-Q9 questions, the
  severity meanings, and the `plan-not-worth-executing` single-finding rule
- `## When to use` and the trigger for dispatching this review
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `plan-audit/SKILL.md`, which is read-only precedent here
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to preserve:
the `PSR-F-###` ID form, the severity set, and the finding classes stay
unchanged, so a manager decides the same findings; `Build required` stays present
and `Residuals` stays last; the shared handoff's required lines are all still
returned. Never embed transcript paths or home paths.

## Acceptance criteria
- The `## Handoff` section, read on its own, admits exactly one listing of each
  finding, and each finding is one line
- A finding whose alternatives or cost comparison do not fit one line has a
  stated place to put that prose and a stated way to cite it
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  file; plan validate exits 0
