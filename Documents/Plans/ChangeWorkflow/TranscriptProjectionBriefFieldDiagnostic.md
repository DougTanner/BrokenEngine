<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T21:17:59.137Z","dependsOn":[]} -->
# Fix: Get-TranscriptProjection.ps1 — briefs missing required fields yield a silent zero-match projection

## Context
Running `pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath <run transcript>` over a `/next-plan` run that contained three delegation briefs printed `use`, `result`, and `assistant-text` rows but zero `match` rows. The three briefs carried no `Governing paths:` line and two carried no `Role:` line, so the brief-path detector never treated any of them as a delegation record.

The detector is at `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:65-76`: a string input field is skipped when its first non-blank line does not start with `Role:` (line 68), and skipped again when no `Governing paths:` line is present (line 71). The script's header comment states the same requirement at `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:7-9`. Both skips are silent `continue` statements, so a run whose briefs are malformed is indistinguishable in the output from a run whose briefs genuinely reused nothing main had read.

The forced rework: the checkpoint reviewer could not tell whether the empty projection meant "no reuse" or "detector never fired", and had to open each delegation record by hand to run the isolation lens that the projection exists to serve.

The symptom has two sides. The main session's briefs omitted mandatory `## Task brief` fields (`Role:` and `Governing paths:` among the fields `.agents/references/subagent-reporting.md` requires). That conformance side is owned by `.agents/references/subagent-reporting.md` and is out of scope here. This Plan covers only the script side: a malformed brief is reported as an empty result instead of as a diagnostic naming the brief and the field it lacks.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6701cd5b-92e8-4e15-a011-bbf833c743d0
- Worktree/branch UUID: 9a52680c-68c7-4af3-bb25-45e7272ef4bc
- Session branch: claude/9a52680c-68c7-4af3-bb25-45e7272ef4bc
- Worktree: .claude\worktrees\BrokenEngine\9a52680c-68c7-4af3-bb25-45e7272ef4bc
- Landing ref: claude/9a52680c-68c7-4af3-bb25-45e7272ef4bc — the observing session records and lands this Plan itself, so its session branch tip is that session's final commit, surviving exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone: `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/TranscriptProjectionBriefFieldDiagnostic.md`, but a periodic Plan-history squash can make it return an unrelated aggregate commit, so review its result only when the commit is attributable to one session alone (its diff limited to that session's files); never review an aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above: Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`, which already names the two silent `continue` statements. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/9a52680c-68c7-4af3-bb25-45e7272ef4bc` in bounded friction mode, supplying the recorded client `claude` and the recorded conversation session ID above. Then make the smallest fix inside the `## In scope` boundary below.

Recommended shape, for the fix session to confirm or replace: keep the existing `match` rows unchanged and add one row per delegation-shaped input field that fails the detector's field requirement, naming the transcript line, the field, and the missing field name — for example `<line> brief-missing <field> <missing field name>`. A field is delegation-shaped when it satisfies one of the two requirements but not the other, which distinguishes a malformed brief from ordinary prose that was never meant to be a brief. Recommended rationale: the reviewer needs to distinguish "detector never fired" from "no reuse", and an extra row type costs nothing when no brief is malformed. The script's header comment documents every row it prints, so the new row belongs there too.

If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1` — the detector and its header comment; this file is the authorized fix boundary
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` and its `references/` — read only to confirm how the projection's rows are consumed; change only if a new row type must be described where the reviewer reads it

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the brief-path detector at `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:65-76` and the header comment's row list at `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:1-14`
- Whatever minimal text in `.agents/skills/next-plan-checkpoint-review/` describes the projection's rows to the reviewer, if a new row type is added

## Out of scope
- Brief conformance in the main session — the omission of `Role:`, `Governing paths:`, and any other mandatory `## Task brief` field is owned by `.agents/references/subagent-reporting.md` and is a separate behavior fix
- The brief-path matching rule itself, the read allowlist, and the `use`/`result`/`assistant-text` row formats
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's output, per `.agents/references/risk-tiers.md`); escalate if the fix reaches build/bootstrap coordination. The script stays read-only and writes its whole result to stdout. Never embed transcript paths or home paths.

## Acceptance criteria
- A transcript containing a delegation-shaped brief that lacks `Role:` or `Governing paths:` produces a row naming that brief's transcript line and the missing field, instead of no output for that record
- A transcript whose briefs carry both fields produces the same `match`, `use`, `result`, and `assistant-text` rows as before the change
- The header comment's row list describes every row the script can print
- The static-checks runner, invoked as `.agents/references/static-checks.md` documents it, reports every row the change triggers passing
