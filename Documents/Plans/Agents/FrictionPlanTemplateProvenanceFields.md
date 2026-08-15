<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T13:03:10.826Z","dependsOn":[]} -->
# Fix: create-follow-up-plans SKILL.md — friction Plan template records an unnamed `Session` field and a landing-commit recipe a history squash defeats

## Context

`.agents/skills/create-follow-up-plans/SKILL.md` owns the tooling-friction Plan
body template (the fenced `markdown` block under `### 2. Draft and classify`,
currently `:51-97`) that every friction follow-up is drafted from. That template
still carries the field set that existed before `/next-plan` split the two
session identifiers apart, so a Plan drafted from it today records provenance
`/next-plan-review` cannot act on.

Root cause, verified in the current tree:

- `.agents/skills/create-follow-up-plans/SKILL.md:60` emits a bare
  `- Session: <lowercase uuid>` line that never says which identifier it holds.
  `.agents/skills/next-plan/SKILL.md` (`## Tooling friction follow-ups`,
  currently `:154-168`) now requires two separately named values: the
  worktree/branch UUID, and — on Claude only — the conversation session ID,
  which main reads from the `CLAUDE_CODE_SESSION_ID` environment variable in its
  own session shell (a subagent shell reports that subagent's own ID). Codex
  records no conversation session ID because `/next-plan-review` discovers Codex
  transcripts by bounded commit window. The template has no field for the
  conversation session ID at all.
- `.agents/skills/next-plan-review/SKILL.md` (friction-provenance expectation
  paragraph, currently `:68-71`) now counts only a recorded *conversation*
  session ID as a supplied exact session id, and treats the recorded
  worktree/branch UUID and worktree as selection evidence only. A Plan drafted
  from the current template therefore supplies nothing that satisfies that
  paragraph on Claude.
- `.agents/skills/create-follow-up-plans/SKILL.md:70-72` (`## Design` text) and
  `:81` (`## In scope` text) still say the review session is run "supplying the
  recorded client and session id", wording that predates the split and names no
  identifier.
- `.agents/skills/create-follow-up-plans/SKILL.md:64` records the landing commit
  as the lookup recipe
  `git log --diff-filter=A --format=%H -- <this plan path>`. This repository
  periodically squashes Plan history, which defeats that recipe: run against
  `Documents/Plans/Agents/NextPlanProvenanceIdentityAttribution.md` it returns
  `cd07f0b95dcfb74db9164f6537a2500644e16ae6` ("Plans"), a 129-file aggregate
  squash, not the landing commit that produced that Plan. A review invoked on
  that aggregate commit reads a diff that is almost entirely unrelated to the
  session under review.

Pre-existing and out of scope where observed: this was found while executing
`Documents/Plans/Agents/NextPlanProvenanceIdentityAttribution.md`, whose
`## In scope` covered only `.agents/skills/next-plan/SKILL.md` and
`.agents/skills/next-plan-review/SKILL.md` and whose `## Out of scope` named
"Unrelated skills/scripts". `/create-follow-up-plans` was outside that boundary,
so aligning its template is follow-up work rather than an in-scope blocker.

## Design

Documentation-only edit of the friction template and the prose that immediately
introduces it in `.agents/skills/create-follow-up-plans/SKILL.md`. No script,
schema, or other skill changes. Three decided changes:

1. Split the provenance field. Replace the template's single
   `- Session: <lowercase uuid>` bullet with two named bullets, worded to match
   `/next-plan`'s current terminology:
   - `- Conversation session ID:` — a lowercase UUID on Claude, recorded from
     `CLAUDE_CODE_SESSION_ID` read in main's own session shell (state that a
     subagent shell reports the subagent's own ID); `none` on Codex, because
     `/next-plan-review` discovers Codex transcripts by bounded commit window.
   - `- Worktree/branch UUID:` — the lowercase UUID that also appears in the
     `Session branch` and `Worktree` lines; selection evidence only, never
     production proof.
   Reword the template's closing "Run the review before /cleanup-worktrees"
   bullet so "the exact session id above" reads as the conversation session ID.

2. Align the stale phrasing. In the template's `## Design` paragraph and its
   `## In scope` bullet, replace "supplying the recorded client and session id"
   with wording that names the client and, on Claude, the recorded conversation
   session ID, and states that a Codex review supplies the client and landing
   ref only.

3. Make the landing reference survive a history squash. Replace the
   `- Landing commit: <git recipe>` bullet with a `- Landing ref:` bullet that
   names the recorded session branch (`<claude|codex>/<uuid>`, already a field
   in the block) as the ref to review: its tip is the session's final commit and
   it stays available exactly as long as the worktree the block already warns
   must survive until the review runs. Keep
   `git log --diff-filter=A --format=%H -- <this plan path>` only as an
   explicitly labelled fallback for use after the branch is gone, with a
   one-clause caveat that a periodic Plan-history squash can make it resolve to
   an unrelated aggregate commit, so the returned commit must be confirmed to
   touch this session's files before it is reviewed.

Existing landed friction Plans keep their historical provenance text; this Plan
changes the template only, so future Plans are drafted correctly.

## Critical files

- `.agents/skills/create-follow-up-plans/SKILL.md` — the fenced friction body
  template under `### 2. Draft and classify` and the sentence immediately
  introducing it; no other section.

## In scope

- The provenance-block field split, the `## Design`/`## In scope` phrasing
  alignment, and the landing-ref change described above, all inside the friction
  template and its introducing sentence in
  `.agents/skills/create-follow-up-plans/SKILL.md`.

## Out of scope

- `.agents/skills/next-plan/SKILL.md`, `.agents/skills/next-plan-review/SKILL.md`,
  and every other skill or reference file; this Plan follows their current
  wording, it does not change it.
- Any script, including `.agents/scripts/New-PlanFile.ps1`,
  `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`, and
  the plan scheduler; no behavior change of any kind.
- Rewriting provenance blocks in already-landed friction Plans under
  `Documents/Plans/`.
- Any transcript path, transcript text, or absolute home path in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior, one skill's documented contract);
escalate only if the resulting fix reaches build/bootstrap coordination or a
script. Invariant preserved: the `Worktree` line stays a profile-relative
locator and never an absolute path, and no transcript path or transcript text
enters the repository.

## Acceptance criteria

- The template's provenance block names the conversation session ID and the
  worktree/branch UUID as separate fields, with the Claude-only source and the
  Codex `none` case stated, matching `/next-plan`'s
  `## Tooling friction follow-ups` wording as it then reads.
- A Plan drafted from the amended template supplies exactly what
  `/next-plan-review`'s friction-provenance expectation paragraph counts as a
  supplied exact session id on Claude.
- The template no longer presents `git log --diff-filter=A` as the sole landing
  reference, and its fallback carries the aggregate-commit caveat.
- `/validate-skill` passes for `.agents/skills/create-follow-up-plans/SKILL.md`;
  `plan validate` exits `0`.

## Notes

Root cause is already proven from the file citations above, so no
`/next-plan-review` transcript session is required to start this work.
