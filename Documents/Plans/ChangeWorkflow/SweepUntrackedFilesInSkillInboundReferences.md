<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T12:59:01.503Z","dependsOn":[],"agentCli":"claude"} -->
# Sweep untracked files in the skill inbound-reference sweep

## Context

`/external-skill-creator` validate mode runs the inbound-reference sweep as a
mandatory step and never redoes it by hand
(`.agents/skills/external-skill-creator/references/semantic-review.md`
`## Steps` step 6): the reviewer classifies each returned hit, and the
run-as-evidence check, graded High, rests on that result. The same step runs
when combined-mode `/coherence-review` carries the validate-mode check
(`.agents/skills/coherence-review/references/worker.md` step 7).

`.agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1`
lists its candidate files with `git ls-files`, which reads only the real index,
then skips a listed path that is missing on disk.

A review round's snapshot is HEAD's tree plus every path `git status` reports,
both sides of a rename included, staged in a temporary index, so the real index
is never written (`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header,
`Review rounds` paragraph). A new file the session does not stage therefore
stays untracked through the session until finalize preparation builds the
candidate commit.

Observed consequence: a new untracked skill, reference, or `AGENTS.md` file
that requires the target skill, or chains to it as run-as evidence, is in the
round's snapshot but never listed by the sweep, so the reviewer classifies no
hit for it and the High run-as-evidence check can pass without it. After an
unstaged rename, the old path is listed but missing on disk and skipped, and
the new path is untracked, so references in the renamed file are lost too.
The result reports no error, so the miss is silent.

The target package itself is unaffected: validate mode takes a disk directory
as `Path` (`.agents/skills/external-skill-creator/SKILL.md` `## Inputs`),
`Validate-Skill.ps1` enumerates the package files from disk, and disk matches
the snapshot while the round is open (`.agents/references/subagent-reporting.md`
`## Delegated execution context`, last paragraph).

## Design

The author recommends listing untracked files alongside tracked ones, so the
listed set matches the paths the snapshot holds:

1. Replace `git ls-files` with `git ls-files --cached --others
   --exclude-standard`. `--others --exclude-standard` adds untracked files that
   are not ignored, which are the untracked paths `git status` reports, so the
   new side of an unstaged rename is listed; the existing missing-on-disk skip
   still drops the old side, which the snapshot also deletes. Ignored paths
   (`Temp/`, the gitignored Coretide clone) stay out, and the two lists never
   overlap, so no hit is reported twice.
2. Update the comments that say the sweep covers tracked files (the root-set
   comment above `$script:SweepRoots` and the comment above the listing) to
   say tracked and untracked, non-ignored files.

Reading the snapshot's tree through a `-Head` input is the heavier alternative;
it is not recommended because disk already holds the snapshot's bytes while
the round is open.

Change Workflow tier: Tier 2 — scoped tool behavior: one skill script's file
listing. No determinism, wire, serialization, threading, or trust-boundary
change; no C++, shader, build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1`
  (the `git ls-files` listing in the main `try` block and the comments
  describing tracked files)

## In scope

- The file-listing command in `Find-SkillInboundReferences.ps1` and its
  comment.
- The root-set comment above `$script:SweepRoots` that describes tracked files.

## Out of scope

- `semantic-review.md` step 6 and every other instruction file; their wording
  does not depend on the listing source.
- The root set, the record and output caps, the binary-file skip, and the
  result schema.
- `Validate-Skill.ps1`, `Get-ChangeWorkflowSteps.ps1`, and how main chooses
  the packages it passes as validate-mode `Path`.
- `/comment-review`'s `Session` scan, which reads the review round's snapshot
  through `Find-CommentBlocks.ps1` `-Head`.

## Acceptance criteria

- In the session worktree, with an untracked new file under
  `.agents/references/` holding a line that names a skill, and an unstaged
  rename (`Move-Item`, no `git mv`) of a tracked `.agents/skills/` Markdown
  file that names the same skill, the sweep for that skill returns a record
  for the new file and one for the renamed file's new path, and none for its
  old path.
- With no untracked files, the sweep's records and `rootHitCounts` match the
  baseline script's.
- A file under an ignored path (`Temp/`) naming the skill yields no record.
- `Validate-Skill.ps1` returns `VALID` for `external-skill-creator`.
- The static-checks runner passes for the changed files.

## Notes

Origin: the evidence came from checking, while auditing the follow-up Plans of
the review-round change, whether other review skills share `/comment-review`'s
gap of scanning the working tree instead of the review round's snapshot.
`/verify-acceptance` was checked in the same pass and has no such gap.
