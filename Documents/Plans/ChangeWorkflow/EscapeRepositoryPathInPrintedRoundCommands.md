<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:02:11.320Z","dependsOn":[]} -->
# Escape the repository path in the printed round commands

## Context

`.agents/scripts/Get-ChangeWorkflowSteps.ps1` prints two commands for main to
run: the `-CloseRound` row's scope (the last `$table.Add` in `Get-RoundLines`,
about :234) and the `-StartRound` row's scope (built from
`$command`, about :369). Both embed the repository path as
`-RepositoryRoot '$RepositoryRoot'`, a single-quoted PowerShell string with
the path inserted unescaped. A path containing an apostrophe ends the string
early. A review observed PowerShell rejecting both printed commands for the
repository path `C:\Users\O'Connor\BrokenEngine`.

Root cause: in a single-quoted PowerShell string an apostrophe is written as
two apostrophes, and neither row doubles it.

## Design

Recommended: double every apostrophe of `$RepositoryRoot` where the two rows
build their commands (one shared local value if both rows read the same
path), so the printed text is a valid single-quoted literal of the same path.
Leave the other printed values, which are hex SHAs, keys, and severities,
unchanged.

Change Workflow tier: Tier 2 — scoped tool behavior: the printed text of one
script. No determinism, wire, or trust-boundary change; no C++, shader,
build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/scripts/Get-ChangeWorkflowSteps.ps1`

## In scope

- The `-CloseRound` row's scope string and the `$command` string the
  `-StartRound` row uses.

## Out of scope

- Other scripts that print commands.
- The parameters and behavior of `-StartRound` and `-CloseRound`.

## Acceptance criteria

- In a scratch repository whose path contains an apostrophe, the printed
  `-StartRound` and `-CloseRound` commands, run as printed, parse and run.
- For a path without an apostrophe, the printed commands are unchanged.
- The static-checks runner passes for the changed files.
