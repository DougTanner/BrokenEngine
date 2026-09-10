<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T19:23:10.371Z","dependsOn":[]} -->
# Fix: dispatch briefs — main reads each worker's whole `SKILL.md` to assemble them

## Context
Observed symptom, recorded by a `/next-plan-review` of an earlier session. While
assembling dispatch briefs, main read six worker-facing `SKILL.md` files whole
although a brief needs only the sections `## Design` below names:
`/plan-alternatives` 6,073 characters, `/plan-simplicity-review` 5,816,
`/progressive-disclosure-review` 4,348, `/plan-audit` 4,124,
`/coherence-review` 3,792, and `/finalize-changes` 7,053 — 31,206 characters of
main-session context. The dispatcher-facing sections main actually consumes
(`## When to use` plus `## Inputs` plus `## Handoff`) are 24,027 of those
31,206 characters, so the rule proposed here avoids about 7,179 characters per
such run — the `Purpose` and `References` material the dispatched worker reads
itself — not the whole 31,206. The same session proved the cheap form once,
reading `.agents/skills/external-skill-creator/SKILL.md` with a bounded offset
for 1,822 characters, so the difference is procedure, not tooling capability.

No rule states which part of a skill main reads when dispatching it: the task
brief form in `.agents/references/subagent-reporting.md` `## Task brief` names
the `Required sections`, `Governing paths`, and `Return` fields main must fill,
but does not say how main sources them. Three existing statements are the
candidate owning locations. `.agents/references/change-workflow.md:111` already
states the principle for a cited reference — "main cites that reference in the
brief's `Governing paths` and the `implementer` reads it there, so main need not
read it". `.agents/references/skill-skeleton.md:62-68` keeps
`references/worker.md` private to the executor behind the worker-entry marker,
so a dispatching session never loads it. And
`.agents/skills/next-plan/SKILL.md:71-74` has main leave reading the target
source to the dispatched `implementer`. None of them states the rule for a
skill body main is dispatching.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e6a8bdcc-562d-4d35-9ed0-416a914b687d
- Worktree/branch UUID: cf9c7025-5876-47d6-97be-085f5dc41b2d
- Session branch: claude/cf9c7025-5876-47d6-97be-085f5dc41b2d
- Worktree: .claude/worktrees/BrokenEngine/cf9c7025-5876-47d6-97be-085f5dc41b2d
- Observed in an earlier session: the fields above are the observing session's,
  not the recording session's; that session landed commit
  a4d29b504622c2f2a567140ef6fe537f8024fdff.
- Landing ref: claude/637a7f16-f462-48b6-9ea7-79aa886b3d95, the recording
  session's branch.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/DispatchBriefSectionReads.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`:
confirm that every worker-facing `SKILL.md` a dispatch targets carries the
sections a brief actually consumes, and that no other brief field is sourced
from the skill body. Only when the transcript is genuinely needed, in a new
session run `/next-plan-review a4d29b504622c2f2a567140ef6fe537f8024fdff` in
bounded friction mode, supplying the recorded client `claude` and the recorded
conversation session ID above.

Then make the smallest fix inside the `## In scope` boundary below. The author's
recommendation is a single sentence in the dispatch procedure of
`.agents/references/subagent-reporting.md` `## Task brief`, stating that main
reads only `## When to use`, `## Inputs`, and `## Handoff` of the target skill
and leaves the rest of the skill to the dispatched worker, the same division
`.agents/skills/next-plan/SKILL.md` already applies to Plan-cited source.
`.agents/references/skill-skeleton.md:56-60` lists the sections a subagent
skill's `SKILL.md` carries and states at `:58` that `SKILL.md` is read by the
dispatcher and the executor; it does not itself mark a dispatcher-facing subset,
so this Plan chooses those three as the sections main reads — text main must
present or ask verbatim being a `## Handoff` subsection per `:60` — and leaves
`Purpose` and `References` to the worker. The rationale is that the brief fields
are derived only from those sections, so reading the body whole buys main
nothing it can act on. A bundled script that extracts those sections is the
alternative; the author recommends against it as new machinery for a rule a
reader can follow with a bounded read. If root-causing shows the fix lies
outside the boundary below, surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/references/subagent-reporting.md` — `## Task brief`
- `.agents/references/change-workflow.md` — the cited-reference sentence at
  `:111` under the Run targeted pre-review checks step, only if the rule's
  single owning location proves to be there instead
- `.agents/references/skill-skeleton.md` — the dispatcher-facing/private
  section split, and the same alternative owning location

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the dispatch procedure in
  `.agents/references/subagent-reporting.md` `## Task brief`, or to the
  cited-reference sentence at `.agents/references/change-workflow.md:111` or
  the section split in `.agents/references/skill-skeleton.md` when one of those
  is the owning location — stated exactly once and referenced from the others

## Out of scope
- The landed change the observing session produced
- The dispatcher-facing sections of any skill package
- The delegation role table and the handoff form in
  `.agents/references/subagent-handoff.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior — one reference's documented procedure);
escalate if the fix reaches root `AGENTS.md` or a skill package's own contract.
Invariants: the rule keeps exactly one owning location; every brief field named
in `## Task brief` remains sourceable after the change; no transcript path or
home path is embedded.

## Acceptance criteria
- The reference states, exactly once, which sections of a target skill main reads
  when assembling a dispatch brief, and the other locations reference it
- Every field of the `## Task brief` form is still sourceable from the named
  sections plus the session context, with any exception named in the text
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `markdown-links` row passing, and the `validate-skill` row passing for any
  changed package
