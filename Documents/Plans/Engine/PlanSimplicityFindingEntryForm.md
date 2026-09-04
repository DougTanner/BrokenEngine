<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T22:48:51.631Z","dependsOn":[]} -->
# Fix: `/plan-simplicity-review` — mandated finding entry is a paragraph, so each finding is delivered twice

## Context
Observed while running Change Workflow Step 2 for a `/next-plan` claim in this
session: the dispatched `reviewer` returned finding `PSR-F-001` as a long inline
block that pasted a grep census, a precedent comparison, and the cost comparison,
and then repeated a condensed restatement of the same finding under the shared
handoff's `Findings:` field. One finding arrived twice, and main had to read both
copies to confirm they were the same finding before deciding on it.

The emitter is `.agents/skills/plan-simplicity-review/SKILL.md:72-92`. The
mandated per-finding entry form there is a single multi-clause paragraph carrying
class, concrete problem, evidence, occurrence/likelihood, simpler alternative(s),
cost comparison, and disposition. `:92` then states "Those entries are the rows
of the shared handoff's `Findings` field", while the rule that field must satisfy
is in `.agents/references/subagent-reporting.md` `## Handoffs`: "Every row is one
line. Do not quote code and do not repeat a row from another field", with the
whole handoff capped at 40 lines or 20,000 characters. A paragraph-shaped entry
cannot be a one-line row, so a reviewer following the entry form and a reviewer
following the shared rule produce different output, and the observed run produced
both.

`SKILL.md:100-103` already routes per-question judgment notes to a gitignored
`Temp/` file cited under `Evidence`, but only for a clean `PASS` result, so a
run with findings has no stated place for the Q1-Q9 reasoning.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: e992db79-2417-4700-bdf8-4ac6f0b5b498
- Worktree/branch UUID: a0377e9a-976d-4705-9ecc-b9a32413ba33
- Session branch: claude/a0377e9a-976d-4705-9ecc-b9a32413ba33
- Worktree: .claude\worktrees\BrokenEngine\a0377e9a-976d-4705-9ecc-b9a32413ba33
- Landing ref: claude/a0377e9a-976d-4705-9ecc-b9a32413ba33
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/PlanSimplicityFindingEntryForm.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the two cited sections are visible without a transcript, so a transcript should
not be needed. Only when it is genuinely needed, in a new session run
`/next-plan-review claude/a0377e9a-976d-4705-9ecc-b9a32413ba33` in bounded
friction mode, supplying client `claude` and the conversation session ID above.

The author's recommendation, offered as a starting point rather than a binding
decision, is to make the entry form satisfiable as a `Findings` row: keep the
`PSR-F-###`, severity, plan locator, class, concrete problem, disposition, and a
single evidence citation as path plus selector on one line, and move the
occurrence/likelihood, simpler-alternative, and cost-comparison reasoning to the
gitignored `Temp/` file `:100-103` already sanctions, extending that allowance
from clean results to results with findings. That removes the second copy by
removing the reason to write one, without weakening what the reviewer must
decide. An alternative worth weighing at Step 1 is to leave the entry form long
and instead state at `:92` that `Findings` carries only the one-line summary with
the full entries cited under `Evidence`; it keeps more text in the handoff and is
the smaller edit, but it leaves the same content in two places by design.

Then make the smallest resulting fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary — for example that the
shared handoff rule itself is what should change — surface it for re-planning
instead of expanding scope.

## Critical files
- `.agents/skills/plan-simplicity-review/SKILL.md`
- `.agents/references/subagent-reporting.md` (read-only authority for the
  `Findings` row rule)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/plan-simplicity-review/SKILL.md`: the per-finding entry form at
  `:72-87`, the sentence at `:92` binding those entries to the `Findings` field,
  and the `Temp/` evidence allowance at `:100-103`
- `.agents/skills/plan-simplicity-review/references/worker.md` only where it
  restates the changed entry form

## Out of scope
- `.agents/references/subagent-reporting.md` `## Handoffs`, which is the
  authority this Plan conforms to, not a file this Plan changes
- The Q1-Q9 review questions themselves, the finding classes, the severity
  vocabulary, and when the skill is dispatched
- `/plan-audit`'s unbounded `Traceability checked:` field, owned by
  `Documents/Plans/Engine/PlanAuditTraceabilityBound.md`
- Any handoff-format validator or script; this is a prose fix
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the mandated output contract of one
review skill, which parents consume); this author's classification, for main to
confirm at Step 1. It drops to Tier 1 only if the resulting edit provably changes
no content a reviewer must emit. Invariants to preserve: every fact the current
entry form requires a reviewer to decide — class, concrete problem, occurrence
evidence, simpler alternative, cost comparison, disposition — still reaches the
manager somewhere; the `plan-not-worth-executing` sole-finding rule at `:89-90`
stays; `Findings` rows keep the shared `ID Critical|Required|Recommended
path:line — claim — evidence` shape; no transcript path or home path is embedded.

## Acceptance criteria
- The recorded symptom no longer reproduces: following `SKILL.md` verbatim for a
  plan with one finding yields exactly one delivery of that finding, and the
  entry as written fits one line
- `Validate-Skill.ps1` reports `VALID` for the changed `SKILL.md`; plan validate
  exits 0
