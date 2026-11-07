<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T15:38:22.337Z","dependsOn":[]} -->
# Fix: /finalize-changes — the owned-path list is bounded by the Windows command line

## Context

`/finalize-changes` passes the session's whole landing set to two scripts as one
comma-joined `-OwnedPaths` token on a `pwsh -NoProfile -File` command line:

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`
  declares `[string[]] $OwnedPaths` (line 12) and splits the single token on
  commas (lines 60-61, `$declared`).
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
  declares `[string[]] $OwnedPaths` (line 11) and splits it the same way in
  `Get-OwnedPathList`.
- `.agents/skills/finalize-changes/references/scripts.md` documents both
  invocations with `-OwnedPaths '<path>,<path>'` (`## Invocation`), and its
  `Invoke-FinalizeCandidateCommit.ps1` bullet states that several owned paths
  can only travel as one comma-separated token.

Root cause: the whole list is part of the process command line, and Windows
caps a command line at 32,767 characters, so a large enough landing set fails
before either script runs. The scripts' own handling is not the bound: the
candidate script stages through `git add --pathspec-from-file`, and its other
Git calls take one path each.

Evidence: a landing of regenerated island and texture data carried 467 owned
paths, about 29,000 characters comma-joined, close to the cap. That landing's
first finalize attempt failed with `The filename or extension is too long.` at
the candidate script's own `git add`, which then took every path as an argument;
moving that staging to a pathspec file fixed it. The caller-side
`-OwnedPaths` argument is the bound that remains, and a larger
regenerated-data landing will exceed it.

`Documents/Plans/AGENTS.md` `## Plan files` forbids commas in support-file
names because "the landing splits its owned paths on commas"; that rule exists
only for this token.

## Design

The author recommends replacing `-OwnedPaths` in both scripts with one
`-OwnedPathsFile '<path>'` parameter: a BOM-less UTF-8 file under `Temp/`
holding one repository-relative path per line. The finalize worker writes it
once per preparation and passes the same file to both scripts. Rationale: a
file has no length cap, needs no shell quoting in Git Bash or PowerShell, and
matches how the candidate script already hands paths to Git. Newline over NUL
separation is recommended because the worker writes the file with ordinary
text tools; both scripts keep their current per-path validation
(`Normalize-Path`, `Assert-FinalizeGitPath`), duplicate removal, and empty-list
errors, applied to the file's lines.

Remove `-OwnedPaths` outright; keep no comma form. Because nothing then splits
owned paths on commas, delete the comma rule for support-file names from
`Documents/Plans/AGENTS.md`. Neither result schema changes.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
- `.agents/skills/finalize-changes/references/scripts.md`
- `Documents/Plans/AGENTS.md`

## In scope

- `Invoke-FinalizeCandidateCommit.ps1`: the `param` block's owned-path
  parameter and the line-60 comment and line-61 parse that build `$declared`
- `Invoke-FinalizePrimaryMovementCheck.ps1`: the `param` block's owned-path
  parameter and `Get-OwnedPathList` with its call site
- `scripts.md`: the two invocation lines in `## Invocation` and the
  `Invoke-FinalizeCandidateCommit.ps1` bullet's single-token sentence, plus a
  statement of the file's format and that the worker writes it
- `Documents/Plans/AGENTS.md` `## Plan files`: the comma sentence for
  support-file names
- Any other `/finalize-changes` instruction text that tells the worker how to
  pass the owned paths, found by searching `.agents/` for `OwnedPaths`

## Out of scope

- The candidate script's staging, rollback, and postcondition logic, and its
  `broken-engine-finalize-candidate/v5` result
- The primary-movement check's assessment logic and result schema
- Which paths belong to the landing set
  (`.agents/skills/finalize-changes/references/worker.md` step 1 and the
  landing acceptance table)
- `New-PlanFile.ps1 -DependsOn` and other scripts' comma-token parameters
- Other landing scripts and WorktreeCli

## Risk tier and invariants

Tier 2, trigger: scoped tool behavior of one subsystem, the input contract of
two `/finalize-changes` scripts and its documented invocation. No trust
boundary, serialization, determinism/CRC, wire, replay, affinity, threading,
allocation, shader, or build surface is exposed, and no `/agent-harness` run
applies. Invariants: the candidate still stages only the declared paths and
preserves disjoint state; an already-committed deletion in the set stays a
satisfied no-op; an invalid, empty, or unmerged owned path still blocks with its
current code.

## Acceptance criteria

- Both scripts, invoked as `scripts.md` documents, accept an owned-path file
  whose paths joined by commas exceed 32,767 characters and validate every path
  in it (the read-only primary-movement check can show this directly; the
  candidate script in a disposable clone)
- An ordinary landing through `/finalize-changes` succeeds with the new
  invocation
- `.agents/` and `Documents/Plans/AGENTS.md` hold no remaining `-OwnedPaths`
  comma-token instruction or comma rule tied to it
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes

- `Invoke-FinalizePrimaryMovementCheck.ps1` receives the same landing set as
  the candidate script, so both are fixed together.
