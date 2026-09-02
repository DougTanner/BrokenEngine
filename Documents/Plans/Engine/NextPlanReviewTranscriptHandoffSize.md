<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-01T23:56:46.008Z","dependsOn":[]} -->
# Fix: /next-plan-review — fresh transcript analysis handoff floods main context

## Context
A `/next-plan` run checkpoint measured the main session's tool results with
`.agents/skills/context-efficiency-review/scripts/Measure-SessionContext.ps1`.
The envelope verdict was `needs-review` with one over-threshold tool result: a
`reviewer` handoff of 22,473 characters against the checkpoint's
20,000-character per-result threshold. That handoff was the fresh `reviewer`
dispatched by the "Fresh transcript analysis" section of
`.agents/skills/next-plan-review/SKILL.md` (selector: the dispatch whose brief
begins `Role: reviewer — fresh transcript analysis for /next-plan-review`).

The emitter is that section's handoff contract. It already extends the shared
handoff form with `Decisive checks`, `Control-work evidence`, and
`Model-routing evidence`, and the last of those requires a cited inventory row
for every direct child and headless attempt — 47 children in this run — while
the caller separately asked for a root-cause narrative. The contract states
those requirements in prose without bounding their form, so the returned
handoff came back as free prose containing quoted code and narrative repeated
from earlier sections, and exceeded the manager-context budget. The full
reviewer output for this run was retained at a gitignored `Temp/` path (cited by
path only, not carried into this Plan).

The reviewer proposed, as a starting point rather than a decided design, that
the contract require terse tabular rows — a fixed-width row per required child
or headless route, aggregate timing totals rather than per-interval prose, and
short root-cause event rows (roughly seven) that keep session and record
selectors — and that it forbid quoted code and repeated narrative in the
handoff, allowing the full critique to live in a `Temp/` file the handoff names
by path. The implementing session should weigh that against the simpler option
of only tightening the existing bullets to forbid quotation and repetition, and
must keep every conclusion's session-ID-plus-timestamp or event-location
citation, which the parent uses to confirm cited ranges without rereading
transcripts.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6226c3cf-a833-4188-a092-9e0c8e32a254
- Worktree/branch UUID: 922e6c92-4fb0-401c-a63d-36e6e560ccba
- Session branch: claude/922e6c92-4fb0-401c-a63d-36e6e560ccba
- Worktree: .claude\worktrees\BrokenEngine\922e6c92-4fb0-401c-a63d-36e6e560ccba
- Landing ref: claude/922e6c92-4fb0-401c-a63d-36e6e560ccba
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanReviewTranscriptHandoffSize.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
In a new session, run
`/next-plan-review claude/922e6c92-4fb0-401c-a63d-36e6e560ccba`, supplying the
recorded Claude client and the recorded conversation session ID. Root-cause the
friction from the proven transcript, then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan-review/SKILL.md` — section "Fresh transcript
  analysis", including its extended handoff block and the paragraph that follows
  it

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded Claude
  client, the landing ref named in `## Design`, and the recorded conversation
  session ID
- The smallest resulting fix, confined to the "Fresh transcript analysis"
  section of `.agents/skills/next-plan-review/SKILL.md`

## Out of scope
- The landed change the session produced
- The shared handoff form in `.agents/references/subagent-reporting.md` and the
  measurement rules in `.agents/skills/next-plan-review/references/measurement.md`
- The checkpoint's own threshold and `Measure-SessionContext.ps1`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Every transcript conclusion must still cite its
session ID and timestamp or event/line location, delegation-compliance
conclusions must still cite the relevant task brief, and the handoff must still
carry the per-child/headless routing inventory and the control-work aggregates
in a form the parent can confirm against Git and repository artifacts without
rereading transcripts. Never embed transcript paths or home paths in tracked
files.

## Acceptance criteria
- A `/next-plan-review` run over a comparably sized session produces a fresh
  transcript-analysis handoff measured under the checkpoint's
  20,000-character threshold, with every required routing row and citation still
  present
- Any critique detail the handoff omits remains recoverable from a location the
  handoff names
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes
No dependency or Coordination edge is recommended: this tooling defect is
independently landable and must not depend on the Plan the observing session is
completing.
