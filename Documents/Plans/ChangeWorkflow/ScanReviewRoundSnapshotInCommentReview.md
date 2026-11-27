<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:32:27.071Z","dependsOn":[]} -->
# Scan a review round's snapshot in `/comment-review`

## Context

A review round's snapshot is HEAD's tree plus every path `git status` reports,
staged in a temporary index, so the real index and the working tree are never
written (`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header, `Review rounds`
paragraph). Each review row names that snapshot as the `Head` the review reads
(`.agents/references/change-workflow.md` `#### Step 6 — Review and resolve
correctness`), and the round's close records the `/comment-review` result as
covering the snapshot.

`/comment-review` does not read that `Head`:

- `.agents/skills/comment-review/SKILL.md` `## Inputs` takes `Scope` and
  `Baseline` ("plus any untracked paths the review must cover"), and no `Head`.
- `.agents/skills/comment-review/references/worker.md` step 1 runs `Session`
  mode with `-RepositoryRoot` and `-Baseline`, adding `-IncludeUntracked` only
  when the caller supplied untracked files.
- `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` `Session`
  mode builds `$inventoryParams` from `RepositoryRoot`, `Baseline`, `Regions`,
  and the optional `IncludeUntracked` and `PathPrefix`, with no `Head`, so
  `Get-SessionChangeInventory` reads the working tree and the real index
  against the baseline.

Observed consequence: a new C++ or shader file the session has not staged is
in the snapshot but not in the scan when the brief lists no untracked paths,
and the review can return `PASS` without scanning it. Because the snapshot never
stages into the real index, a new source file stays untracked through the
session until finalize preparation builds the candidate commit, so this affects
every session that adds a C++ or GLSL file. Supplying `-IncludeUntracked` alone
admits the new file, but an unstaged rename still reads as a whole-file
addition in the working-tree scan, while the snapshot stages both sides of the
rename.

Origin: the scanner's `Session` mode (commit `9d97ffc35`) combined with the
snapshot design of the review rounds (commit `94fa96136`).

`Get-SessionChangeInventory` already reads a commit-valued `-Head`: it then
takes no untracked entries, and for a review-round snapshot it reads the
Coretide side at the snapshot's `Coretide-Snapshot` and `Coretide-Baseline`
fields (`.agents/scripts/SessionChangeInventory.psm1`, the `$workingTreeHead`
branches of `Get-SessionChangeInventory`).

## Design

The author recommends passing the round's snapshot through to the inventory
the scanner already calls, so the scan's files and regions are the frozen
inventory's:

1. `Find-CommentBlocks.ps1` `Session` mode takes an optional `-Head <commit>`
   and adds it to `$inventoryParams`. The file bytes are still read from disk:
   no file is edited while a round is open (Step 6), so the working tree holds
   the snapshot's bytes for every session path.
2. Worker step 1 passes `-Head <the printed row's Head>` in `Session` mode when
   the brief names one, and `-IncludeUntracked` only when it names none.
3. `SKILL.md` `## Inputs` adds `Head` — the snapshot the printed row names — and
   limits the untracked-paths clause to a brief that names no `Head`.

Change Workflow tier: Tier 2 — scoped tool behavior: one skill's scanner and its
instructions. No determinism, wire, serialization, threading, or trust-boundary
change; no C++, shader, build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` (`param`
  block, `Session` branch building `$inventoryParams`)
- `.agents/skills/comment-review/references/worker.md` (step 1)
- `.agents/skills/comment-review/SKILL.md` (`## Inputs`)

## In scope

- The `-Head` parameter of `Find-CommentBlocks.ps1` `Session` mode and its
  pass-through to `Get-SessionChangeInventory`, with the script header's mode
  description.
- Worker step 1's `Session` mode parameters.
- The `Head` input and the untracked-paths clause in `SKILL.md` `## Inputs`.

## Out of scope

- `SessionChangeInventory.psm1` and the step script.
- `WholeFile` mode and the block grammar (`CommentPattern`).
- The other review skills' scope inputs.

## Acceptance criteria

- In a scratch repository with an open round whose snapshot holds an untracked
  new `.cpp` file carrying a `//` block and an unstaged rename of a `.cpp` file
  with one changed line, `Find-CommentBlocks.ps1` in `Session` mode with
  `-Head <snapshot>` scans the new file and reports its block with
  `overlapsRegion` true, and its scanned files and regions match
  `Get-SessionChangeInventory -Head <snapshot> -Regions` for that snapshot.
- Without `-Head`, the scanner's result is unchanged from the baseline.
- `Validate-Skill.ps1` returns `VALID` for `comment-review`.
- The static-checks runner passes for the changed files.
