<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-01T23:45:47.487Z","dependsOn":[]} -->
# Fix: /next-plan-review — bounded friction root-cause request pays full landing-audit cost

## Context
A `/next-plan` run checkpoint recorded this symptom. The claimed Plan
`Documents/Plans/Engine/CodexRemoveItemLiteralPathBlock.md` — a tooling-friction
follow-up of the standard shape — directed the session to run
`/next-plan-review <landing ref>` for the single bounded purpose its `## Design`
states: root-cause one blocked command from the proven transcript. The invoked
skill has only one mode. Its contract
(`.agents/skills/next-plan-review/SKILL.md`) requires the full landing audit:
provenance proof, a sourced chronological landing timeline, a routing inventory
covering every direct child and headless attempt of the proven parent (47 rows
in this run), control-work classification and measurement with lower/upper
bounds, and the nine-concern assessment with its report tables.

The dispatched fresh `reviewer` performed that full audit and returned a handoff
of roughly 17,400 characters, of which the bounded root cause the Plan asked for
was a small fraction. Everything else was audit output nobody had requested.

The same shape recurs by construction. Every tooling-friction Plan that
`/create-follow-up-plans` authors from its inline template emits the same
`## Design` instruction — "In a new session, run `/next-plan-review <review
ref>` ... Root-cause the friction from the proven transcript" — and the same
`## In scope` bullet "Root-cause investigation via /next-plan-review". So every
tooling-friction Plan ever executed pays the full-audit cost for a bounded
question. `Documents/Plans/Engine/` currently holds several such Plans
(for example `CodexRemoveItemLiteralPathBlock.md` and
`CompileBuildEnvelopeHandoffSize.md`), each of which will do the same when
executed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 87505ff8-449d-404c-9278-0c51ed0dcf6f
- Worktree/branch UUID: 85eb54af-ab57-43cd-9444-a68d4c325df6
- Session branch: claude/85eb54af-ab57-43cd-9444-a68d4c325df6
- Worktree: .claude\worktrees\BrokenEngine\85eb54af-ab57-43cd-9444-a68d4c325df6
- Landing ref: claude/85eb54af-ab57-43cd-9444-a68d4c325df6
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanReviewBoundedFrictionMode.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
In a new session, run
`/next-plan-review claude/85eb54af-ab57-43cd-9444-a68d4c325df6`, supplying the
recorded Claude client and the recorded conversation session ID. Root-cause the
friction from the proven transcript, then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

As a starting hypothesis rather than a decision, the author recommends giving
`/next-plan-review` a bounded mode that answers one named recorded friction:
prove provenance, read the proven transcript, root-cause that one friction, and
skip the routing inventory, the control-work measurement, and the nine-concern
assessment unless the invoker asks for the full audit — with the
`/create-follow-up-plans` tooling-friction template then pointing at that bounded
mode. The rationale is that the invoking Plan already names the single question,
so the remaining audit output is unrequested work whose cost recurs on every
tooling-friction Plan. The review should confirm from the transcript whether the
smaller correction is a mode inside `/next-plan-review` or only a narrower
request contract in the template that emits the instruction, and should weigh
adding a mode against the simpler option of the emitting template naming exactly
which sections it needs. Whatever shape is chosen must keep provenance proof and
the untrusted-transcript handling rules mandatory, because those are what make
any transcript conclusion admissible.

## Critical files
- `.agents/skills/next-plan-review/SKILL.md` — the single-mode audit contract,
  its fresh-reviewer handoff extension, the "Reconstruct and assess" concern
  list, and the report template
- `.agents/skills/next-plan-review/references/measurement.md` — the control-work
  and execution-model routing calculus concerns 5 and 6 apply
- `.agents/skills/create-follow-up-plans/SKILL.md` — the inline tooling-friction
  Plan template, whose `## Design` and `## In scope` text emits the
  `/next-plan-review` instruction

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded Claude
  client, the landing ref named in `## Design`, and the recorded conversation
  session ID
- The smallest resulting fix, confined to the audit-scope contract in
  `.agents/skills/next-plan-review/SKILL.md` (its "Fresh transcript analysis"
  handoff extension, "Reconstruct and assess" concern list, and "Report"
  template), the sections of
  `.agents/skills/next-plan-review/references/measurement.md` those concerns
  invoke, and the tooling-friction template `## Design` and `## In scope` text in
  `.agents/skills/create-follow-up-plans/SKILL.md`

## Out of scope
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1` and
  transcript discovery behavior
- `/next-plan` itself, including its run checkpoint and claim lifecycle
- The landed change the session produced
- Rewriting already-authored tooling-friction Plans in `Documents/Plans/`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Any bounded route must still prove transcript
provenance before drawing a transcript conclusion, keep the transcript-as-
untrusted-data rules, keep the required fresh-`reviewer` delegation with its
single task brief, keep every conclusion cited to session ID and
timestamp/event location, and report `BLOCKED` rather than a weakened conclusion
when provenance fails. The full audit must remain available unchanged for a
landing review. Never embed transcript paths or home paths.

## Acceptance criteria
- A `/next-plan-review` invocation made for one recorded tooling friction
  produces the root cause of that friction without the routing inventory,
  control-work measurement, or nine-concern assessment, while an ordinary
  landing review still produces the complete audit
- The tooling-friction Plan template emits an instruction consistent with
  whichever route is chosen, so a future Plan asks only for what it needs
- /validate-skill passes for any changed SKILL.md; plan validate exits 0
