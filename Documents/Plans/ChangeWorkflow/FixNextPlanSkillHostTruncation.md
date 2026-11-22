<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T19:22:40.396Z","dependsOn":[]} -->
# Fix: `.agents/skills/next-plan` — main reads overlapping tail of injected `SKILL.md` after host truncation

## Context

Three runs hit the same friction when the host truncated injected
`.agents/skills/next-plan/SKILL.md` text near the end of the file, prompting
main to reopen and re-read an overlapping tail range to recover the clipped
rules:

- 2026-11-21, `agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b`: selector
  `.agents/skills/next-plan/SKILL.md` lines 260-422; 11,198 chars entered main
  after a 2,349-byte host truncation; bounding: read only lines after the
  truncation point (or prevent truncation).
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector
  `.agents/skills/next-plan/SKILL.md` lines 320-422; 2,349-byte host clipping
  led main to re-read overlapping lines; bounding: read only lines after the
  truncation point (or prevent truncation).
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector
  `.agents/skills/next-plan/SKILL.md` lines 250-416; 1,939-byte host truncation
  ended mid-rule and forced main to reopen tail lines to recover the truncated
  rules.

Current tree observations:

- `.agents/skills/next-plan/SKILL.md` is 416 lines and 26,625 bytes (6,657
  `bt-token-v1` tokens via `.agents/scripts/Measure-Tokens.ps1`). Subtracting
  the observed 1,939-byte truncation puts the host injection cap near 24.7 KB,
  clipping the tail (`## Rules` at `:373-399` and `## References` at `:401-416`).
- `.agents/skills/next-plan/SKILL.md` is a main-session skill
  (`.agents/references/skill-skeleton.md:66-95`), yet currently carries long
  conditional or rare-path detail inline — such as `### Post-checkpoint outcomes`
  (`:231-243`), `## Handoff` (`:245-293`), and `### Implementation approval`
  split/deferral mechanics (`:334-360`) — alongside six reference files under
  `.agents/skills/next-plan/references/`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 5520d05d-dd0b-4061-98a4-ae41008f3e21
- Worktree/branch UUID: 4004df43-5824-4718-822f-007c9e5d0963
- Session branch: agy/4004df43-5824-4718-822f-007c9e5d0963
- Worktree: .agy\worktrees\BrokenEngine\4004df43-5824-4718-822f-007c9e5d0963
- Landing ref: agy/4004df43-5824-4718-822f-007c9e5d0963
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or agy
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context` — confirming the exact host injection budget and identifying which
conditional or subordinate mechanics in `.agents/skills/next-plan/SKILL.md`
should move into `.agents/skills/next-plan/references/` (while preserving the
main-session skill contract in `.agents/references/skill-skeleton.md`) so the
injected `SKILL.md` stays comfortably below the host truncation threshold with
headroom. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client (`agy`) and
conversation session ID (`5520d05d-dd0b-4061-98a4-ae41008f3e21`). Then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — skill body (`:1-416`, 26,625 bytes),
  including `## Steps` (`:36-229`), `### Post-checkpoint outcomes` (`:231-243`),
  `## Handoff` (`:245-293`), `### Implementation approval` (`:305-360`),
  `## Rules` (`:373-399`), and `## References` (`:401-416`).
- `.agents/skills/next-plan/references/` — existing reference files
  (`claim-exceptions.md`, `claim-results.md`, `preparation-brief.md`,
  `run-checkpoint.md`, `tier3-workflow.md`) or a new focused reference file
  receiving relocated rare-path/subordinate detail.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to the candidate relocation and link
  regions of `.agents/skills/next-plan/SKILL.md` (`## Steps`,
  `### Post-checkpoint outcomes`, `## Handoff`, `### Implementation approval`,
  `## Rules`, and `## References`), only the passages receiving relocated
  material and any necessary anchor or link updates in the existing
  `.agents/skills/next-plan/references/` files (`claim-exceptions.md`,
  `claim-results.md`, `preparation-brief.md`, `run-checkpoint.md`,
  `tier3-workflow.md`) or a new focused reference file receiving relocated
  rare-path or subordinate detail, and inbound section-anchor link updates in
  tracked files that link to a moved heading.

## Out of scope

- The landed change the observing session produced.
- Changing `/next-plan` workflow semantics, claim scripts, or WorktreeCli
  behavior.
- Unrelated skills/scripts, C++ or shader code, Visual Studio project files,
  and any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool/workflow behavior — `.agents/skills/next-plan/`;
no determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay, affinity,
threading, allocation, shader, build, or live `/agent-harness` exposure);
escalate if the fix reaches build/bootstrap coordination or independently owned
subsystems. Preserve every `/next-plan` orchestration invariant, section-anchor
link, and `/external-skill-creator` validation rule. Never embed transcript
paths or home paths.

## Acceptance criteria

- `.agents/skills/next-plan/SKILL.md` fits well below the observed ~24 KB host
  injection truncation boundary (e.g., under 22 KB with headroom) so invoking
  `/next-plan` no longer truncates `SKILL.md` or requires a follow-up tail read.
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan`
  passes with exit 0.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing (including `markdown-links` for any relocated
  headings).

## Notes

- Live `/agent-harness` verification does not apply.
- No live Plan dependencies or reciprocal `## Coordination` constraints apply.
