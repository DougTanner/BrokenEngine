<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T13:52:44.280Z","dependsOn":[]} -->
# Fix: /repo-code-review — a PASS handoff overflows the 4,000-character cap with inline trace rows

## Context
Observed symptom: during a `/next-plan` run, main dispatched
`Role: reviewer Skill: /repo-code-review` over a C++ change touching ImGui and
engine input code. The reviewer returned a `PASS` handoff of 6,059 characters
(the run checkpoint's measurement; the reviewer itself reported about 5,360)
against the 4,000-character cap in `.agents/references/subagent-handoff.md`
`## Handoffs`. The excess was multi-sentence ImGui and engine line traces for
each of the brief's four focus areas, carried inline as rows instead of moving
to a `Temp/` file cited under `Evidence`, which the cap rule requires. The
oversized result entered the main session's context whole.

Durable facts in the emitter: `.agents/skills/repo-code-review/SKILL.md`
`## Handoff` declares a `Files reviewed` extension field of one row per path
holding "the regions and affected paths traced", with no length bound, and the
shared rule says text a skill's `## Handoff` requires inline never moves to a
file, even past the cap. The skill names no summary form for a clean review's
per-focus trace evidence.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: c4130ec1-498c-4711-bb9d-66a87c7b9776
- Worktree/branch UUID: 0787cd30-7a64-4520-a5f1-f5cb541c9821
- Session branch: claude/0787cd30-7a64-4520-a5f1-f5cb541c9821
- Worktree: .claude\worktrees\BrokenEngine\0787cd30-7a64-4520-a5f1-f5cb541c9821
- Landing ref: claude/0787cd30-7a64-4520-a5f1-f5cb541c9821 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/RepoCodeReviewHandoffCap.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/0787cd30-7a64-4520-a5f1-f5cb541c9821` in bounded
friction mode, supplying client `claude` and conversation session ID
`c4130ec1-498c-4711-bb9d-66a87c7b9776`; the review settles which handoff field
carried the trace rows. Then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

The author recommends bounding the `## Handoff` so a `PASS` result carries one
summary row per focus area, with the multi-sentence line traces moved to a
`Temp/` file holding one `##` heading per focus area and cited under
`Evidence` by the shared cap rule; the rationale is that the shared rule already
supplies the overflow mechanism and only this skill's unbounded inline field
blocks it. If root-causing shows the skill text already permits that shape and
the overflow was the reviewer's own deviation, the friction is worker conduct:
record that finding and stop without editing.

## Critical files
- `.agents/skills/repo-code-review/SKILL.md` — `## Handoff`, including the
  `Files reviewed` extension field
- `.agents/skills/repo-code-review/references/worker.md` — only the step or
  rule that assembles the handoff, if the fix needs it there

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the section and file named under
  `## Critical files`

## Out of scope
- The landed change the session produced
- `.agents/references/subagent-handoff.md` and its 4,000-character cap
- Other review skills' handoff forms
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: a skill's handoff contract changes);
escalate if the fix reaches build/bootstrap coordination. Findings rows stay
inline. Never embed transcript paths or home paths.

## Acceptance criteria
- A clean `/repo-code-review` over a multi-focus C++ change returns a `PASS`
  handoff under 4,000 characters, with its per-focus traces in a cited `Temp/`
  file
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
