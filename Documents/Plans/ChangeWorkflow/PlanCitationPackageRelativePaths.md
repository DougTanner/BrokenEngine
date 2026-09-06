<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T13:04:05.646Z","dependsOn":[]} -->
# Fix: Test-PlanCitations.ps1 — a citation written relative to its own file is reported unresolved

## Context
`.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` resolves every
backticked citation token against the repository root only. Skill packages
routinely cite their own bundled files with a package-relative token — a
`references/<file>.md`, `scripts/<file>.ps1`, or `examples/<file>.md` token in a
`SKILL.md` or a worker file — and those tokens carry a `/`, so they are scanned
as citations, resolved against the repository root, found absent there, and
emitted as unresolved rows even though the cited file exists next to the citing
file. The auditor then chases a lead that is not a defect.

A representative instance is `.agents/skills/coherence-review/SKILL.md:78`,
whose bundled-reference bullet cites its own worker file package-relatively. A
scan of the tracked markdown corpus during the session that recorded this Plan
counted 85 such backticked tokens across 55 tracked files, and that count is a
floor: three files excluded by the worktree's sparse checkout could not be read.
The same session's own resolved-citation snapshot still showed three rows of
this class after the separator rule below landed.

The behavior was confirmed from the current tree. `Get-RepositoryRelativePath`
at `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1:70-100` joins every
token to `$script:RepoRoot` and returns `$null` only for a rooted token or one
escaping the root; the scan at `:141-157` feeds it every backticked token
matching `$script:CitationPattern` at `:20`; `pathExists` is computed at `:172`
against that single root-relative location; and `Set-CitationPayload` at
`:275-309` emits every record whose `pathExists` is `false`.

This class survived the recently landed grammar change by design: that change
made a token a citation only when it contains at least one `/`, and a
package-relative token does contain one. So it was never inside that change's
boundary.

## Design
The author's recommendation is the smallest resolution change that keeps the
existing behavior first: try the repository root exactly as today, and only when
that location does not exist, retry the same token relative to the directory of
the file being scanned. The record then reports whichever location resolved, in
the existing `path` field, as a repository-relative path. The fallback runs the
same containment check the root path already runs, so a token that canonically
escapes the worktree root still resolves to nothing and is never read, and the
script still reads nothing outside the worktree root.

Reporting stays unchanged in shape: `totalCount` still counts every scanned
record, `omittedCount` and `truncated` still speak about unresolved records
only, and a token that resolves under the citing file's directory simply has
`pathExists` true and is therefore not emitted. No field is added or renamed and
the schema name does not change.

The constraint that makes the fallback unambiguous today was measured during the
session that recorded this Plan: no multi-segment backticked token in the tracked
corpus resolved both at the repository root and relative to its own citing file,
so root-first ordering never hides a different existing file. The implementing
session should re-confirm that on the tree it lands on and, if a token resolves
in both places, report it rather than silently preferring one.

Two smaller points the implementing session should settle in passing rather than
plan around. The input the script scans can be a reviewer scope file rather than
a plan file, in which case the fallback directory is that scope file's own
directory, which is the same rule and needs no special case. And a line number
attached to a token that resolves through the fallback is checked against the
file that resolved, as it is today.

Alternatives not recommended: making package-relative resolution the only rule,
which would break the repository-relative citations that make up most of the
corpus; and trying the citing file's directory first, which would let a bundled
file shadow a real repository-root path and change the meaning of citations that
resolve correctly today.

## Critical files
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` — the resolver
  (`:70-100`), the scan that calls it (`:141-157`), and the `pathExists`
  computation (`:172`)
- `.agents/skills/plan-audit/references/worker.md` — the documented consumption
  of a citation row; in scope only if the fix changes what a row means to the
  auditor

## In scope
- Adding the citing file's directory as a resolution fallback in
  `Get-RepositoryRelativePath`, or at its call site in `Get-CitationRecords`,
  in `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1`
- Passing the scanned file's directory to whichever function performs the
  resolution
- The matching consumption wording in the plan-audit worker file named above,
  only if the fix changes what an emitted row means

## Out of scope
- The citation token grammar at `$script:CitationPattern`, including the landed
  rule that a token must contain at least one `/`, which must not be reopened
- The success-exit status and code of the same script, including its behavior on
  a truncated result, which `Documents/Plans/ChangeWorkflow/PlanCitationTruncatedStatus.md`
  owns
- The caps (`MaximumCitations`, `MaximumTextLength`, `MaximumOutputBytes`) and
  the shed ordering
- The emitted schema name and fields
- The execution-card completeness payload of the same script
- Any audit judgment rule: the script still renders no verdict on a plan
- Rewriting the prose of any existing plan or skill file to change how its
  citations are written

## Coordination
`Documents/Plans/ChangeWorkflow/PlanCitationTruncatedStatus.md` changes the success-exit
status and code of this same script, a region disjoint from the resolution
region this Plan changes. Neither Plan requires the other first, and neither
constrains the other's chosen behavior. Whichever lands second re-reads the
other's landed change and keeps it working.

## Risk tier and invariants
Expected Tier 2 by the root `AGENTS.md` Tier-2 trigger for one subsystem's tool
behavior — the scoped behavior of one review-support script at an existing
boundary, with no format, trust boundary, determinism, wire, or serialization
exposure; escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: the schema name `broken-engine-plan-citations/v1` and every existing
field; only unresolved records are emitted; the script stays read-only, resolves
and reads nothing outside the worktree root, and writes its whole result to
stdout with diagnostics on stderr; the bundled-script rule's canonical
`pwsh -NoProfile -File <repo-relative path>` invocation from the worktree root.
Never embed transcript paths or home paths.

## Acceptance criteria
- A file citing a bundled file relative to its own directory, such as the
  bundled-reference bullet at `.agents/skills/coherence-review/SKILL.md:78`,
  returns no unresolved row for it
- A file citing a repository-relative path that exists still returns no row for
  it, and one citing a repository-relative path that does not exist anywhere
  still returns that unresolved row
- A token that canonically escapes the worktree root still produces no record
- The status, code, schema name, and fields of the result are unchanged, and
  `totalCount` still counts every scanned record
- `/validate-skill` passes wherever the root `AGENTS.md` Apply the triggered
  cleanup step triggers it; plan validate exits 0

## Notes
This is new scope rather than a re-observation: package-relative resolution sat
outside the `## In scope` boundary of every landed citation-script Plan, and the
grammar change that landed most recently deliberately left this class in place.
