<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T13:30:22.427Z","dependsOn":[]} -->
# Fix: /resolve-findings — a resumed round re-returns the whole item table and all prior Decisive checks

## Context
`.agents/skills/resolve-findings/SKILL.md` `## Handoff` (lines about 48-80)
declares one compact item table plus the extension fields, and says "Name each
changed file once", but it states nothing about a round that resumes an earlier
`/resolve-findings` worker with one newly assigned item. With no rule for that
case, each resumed worker returns the full `CR1`-`CRn` item table and the entire
prior `Decisive checks` string that the first round had already delivered to
main.

Observed in the `/next-plan` run recorded below, at that run's checkpoint, by
the `/next-plan-checkpoint-review` reviewer: two resumed `/resolve-findings`
rounds — one adding item `CR4`, one adding item `CR5`, each a single small
citation or count correction — returned handoffs of 4821 and 4970 characters,
each re-entering the full item table and the whole prior `Decisive checks`
content main already held. The context-efficiency measurement verdict for the
run was `pass` (no tool result over the 20000-character threshold), so this is
not an oversized-result defect; the cost is duplicated main-session context
across rounds. The finding class is a fixable defect in the emitter skill's
handoff contract, not worker conduct: the workers matched the form the skill
declares, and the skill declares no narrower form for a resumed round. There is
no unmet acceptance criterion behind it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: dd78f98f-214b-4634-914b-f654e04de9af
- Worktree/branch UUID: fcf82b3f-6ea3-4a9a-a48e-1cfb02414c51
- Session branch: claude/fcf82b3f-6ea3-4a9a-a48e-1cfb02414c51
- Worktree: .claude\worktrees\BrokenEngine\fcf82b3f-6ea3-4a9a-a48e-1cfb02414c51
- Landing ref: claude/fcf82b3f-6ea3-4a9a-a48e-1cfb02414c51 — this Plan is
  recorded and landed by the observing session itself, so that branch's tip is
  that session's final commit and it survives exactly as long as the worktree
  above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
The `## Context` symptom already identifies the missing rule, so the fix session
should root-cause from the current `## Handoff` text rather than from a
transcript. Only if that text turns out to be sufficient as written should the
session run `/next-plan-review claude/fcf82b3f-6ea3-4a9a-a48e-1cfb02414c51` in
bounded friction mode, supplying client `claude` and the conversation session ID
above.

The author's recommendation is to add one rule to `## Handoff` covering a
resumed round: a round that resumes an earlier `/resolve-findings` worker with a
newly assigned item returns only that item's own table row, the `Changed files`
delta this round produced, and its own `Decisive checks` rows, and cites
already-delivered items by their IDs instead of repeating their rows, checks, or
evidence. The extension fields keep their existing shapes and report this
round's state. Keep the wording to the smallest addition that makes the resumed
case unambiguous, and leave the first round's contract exactly as it is today.

Make the smallest fix inside the `## In scope` boundary below. If the fix would
reach the shared handoff form in `.agents/references/subagent-handoff.md`,
surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/resolve-findings/SKILL.md` (`## Handoff`, lines about 48-80)
- `.agents/references/subagent-handoff.md` (`## Handoffs`) — read as the
  authoritative shared form; changing it is out of scope

## In scope
- `.agents/skills/resolve-findings/SKILL.md` `## Handoff`: the sentence "Return
  the shared handoff form in `../../references/subagent-handoff.md`, extended
  with one compact item table and these fields:" together with the fenced item
  table below it, the sentence "Each shared `Decisive checks` row names the
  item, focused check, and result.", and the sentence "Name each changed file
  once." — these are the sentences that define what a round returns and are the
  sentences the resumed-round rule must qualify.
- Adding the resumed-round rule as `## Design` describes it, inside that same
  `## Handoff` section.

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared form's fields or caps
- Every other section of `.agents/skills/resolve-findings/SKILL.md`, including
  its intent classification, acceptance, and `PLAN DELTA REQUIRED` rules
- Any other skill package, and the manager-side dispatch rules that decide when
  a round is resumed
- Any transcript path or transcript text in the repository

## Risk tier and invariants
Tier 1 (mechanical): skill documentation shaping a handoff contract, with no
code, public signature, determinism, serialization, or build exposure. Escalate
only if the fix reaches the shared handoff form. No unit tests. Never embed
transcript paths or home paths.

## Acceptance criteria
- `.agents/skills/resolve-findings/SKILL.md` `## Handoff` states what a resumed
  round returns, distinct from a first round, and the resumed shape excludes
  already-delivered item rows and already-delivered `Decisive checks`.
- `/external-skill-creator` validation of the changed skill package reports no
  new finding.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing.

## Notes
Recorded as a context-efficiency follow-up from a `/next-plan` run checkpoint;
the cost is duplicated main-session context, not a failed acceptance criterion.
`Documents/Plans/ChangeWorkflow/SkillHandoffEnumerationSharedCap.md` edits
different sentences in the same `## Handoff` section of the same file, so
whichever lands second should re-read that section before editing; neither
blocks the other.
