<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T00:52:31.484Z","dependsOn":[]} -->
# Make `-BaseCommit` mandatory in the code-quality history script

## Context

`.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`
declares `-BaseCommit` as an optional parameter (`:8`) and keeps two fallbacks
for the case where a caller omits it:

- `:521` defaults the base commit to `HEAD` (`$base = if ($BaseCommit) {
  $BaseCommit.ToLowerInvariant() } else { $head }`).
- `:527` passes `$null` to `Read-History` when `-BaseCommit` is absent, and
  `Read-History` (`:143`-`:152`) then reads the working-tree
  `CodeQualityMetricsHistory.jsonl` bytes instead of the immutable
  `<BaseCommit>:<path>` Git blob.

The comment immediately above that call (`:525`-`:526`) states the purpose
outright: "Production always supplies BaseCommit so the source table comes from
that immutable Git object. The working-tree fallback is retained only for
standalone fixture mode."
`.agents/skills/code-quality-metrics/references/HistoryContract.md:26`-`:27`
repeats it: "A no-base invocation is retained only for standalone fixture mode
and compares the current working tree (including staged changes) with `HEAD`."

At baseline `690b8651` no tracked caller uses that mode. A repository-wide
search for `Invoke-CodeQualityMetricsHistory`, excluding `Temp/`, matches only
the script itself, its own `references/worker.md`, `references/HistoryContract.md`,
`references/history/README.md`, `.agents/skills/finalize-changes/references/scripts.md`,
and the single production caller
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:204`, which
always passes `-BaseCommit <primary tip>` and `-TipCommit <source commit>`. The
script fixture suites that would have exercised the no-base mode were deleted in
earlier commits and in the session that recorded this follow-up. The fallback
therefore exists only for a caller that does not exist.

This is the same class of leftover that commit `f30b6740` ("Remove fixture-only
hooks from the finalize scripts") cleared out of the finalize scripts, and the
same class as the fixture-only `-FrameSource` parameter removed in the recording
session. It was held out of that session because removing it makes `-BaseCommit`
mandatory, which is a parameter-contract change on a script the landing flow
runs, rather than the pure deletion of unused fixture plumbing the recording
session was approved for.

## Design

The author recommends making `-BaseCommit` mandatory and deleting both
fallbacks, because the root `AGENTS.md` minimum-sufficient-change directive keeps
only mechanisms something uses, and a silent fallback to working-tree bytes is
also the failure mode the history contract exists to prevent: a mistyped or
omitted base would produce a receipt bound to mutable local bytes instead of an
immutable blob, and would do so without any error.

Recommended shape:

- Mark `-BaseCommit` `[Parameter(Mandatory = $true)]` in the param block, so
  PowerShell rejects an omitted value before any work happens.
- Replace `:521` with the unconditional
  `$base = $BaseCommit.ToLowerInvariant()`.
- Replace `:527` with `$history = Read-History $repository $base`, and drop the
  explanatory comment at `:525`-`:526`, which describes a distinction that no
  longer exists.
- Narrow `Read-History` to a non-null `[string]$Base`: remove the
  `[AllowNull()]` attribute and the `if ([string]::IsNullOrWhiteSpace($Base))`
  branch with its working-tree read at `:145`-`:148`, keeping the
  `Assert-CommitObject` plus `Invoke-GitBytes` blob read as the only path.
- Delete the fixture-mode sentence at `HistoryContract.md:26`-`:27` and adjust
  the surrounding wording so the section states plainly that `-BaseCommit` is
  required and always names the source blob.

`-TipCommit` keeps its own `HEAD` fallback (`:522`). It is a different question
— it selects the diff endpoint rather than the source-table bytes, and no
comment marks it as fixture-only — so it is deliberately left alone here.

The implementing session should re-run the caller search at its own baseline
(repository-wide `Invoke-CodeQualityMetricsHistory`, excluding `Temp/`) before
editing. If a no-base caller has appeared since, the correct outcome is to close
this Plan as stale rather than remove the fallback.

Verification is the production path itself: a `-Mode Contract` run with an
explicit `-BaseCommit` and `-TipCommit` must still emit its
`broken-engine-code-quality-history-contract/v1` receipt unchanged, and a run
omitting `-BaseCommit` must be rejected by the parameter binder rather than
silently reading the working tree.

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`
  — param block `:8`, `$base` selection `:521`, comment and `Read-History` call
  `:525`-`:527`, `Read-History` body `:143`-`:152`
- `.agents/skills/code-quality-metrics/references/HistoryContract.md` — the
  fixture-mode sentence at `:26`-`:27`
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:204` —
  read-only; the sole production caller, which already passes `-BaseCommit`

## In scope

- Making `-BaseCommit` a mandatory parameter of
  `Invoke-CodeQualityMetricsHistory.ps1`
- Removing the `HEAD` default for `$base` and the `$null`-base argument at the
  `Read-History` call site, plus the fixture-mode comment that explains them
- Removing `Read-History`'s null-base working-tree branch and its `[AllowNull()]`
  attribute
- Updating the `HistoryContract.md` invocation section so it no longer documents
  a no-base fixture invocation

## Out of scope

- Any change to `-TipCommit`, its `HEAD` fallback, or any other parameter
- Any change to `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
  or to any other finalize script; the production caller already supplies
  `-BaseCommit`
- Any change to the receipt schemas, the immutable prefix contract, the patch
  classifier, the capture/drift logic, or the SVG output
- Any change to `.agents/skills/code-quality-metrics/references/history/README.md`
  or `.agents/skills/finalize-changes/references/scripts.md`; their sentences
  about production binding to the supplied `BaseCommit` stay true afterwards
- Re-adding any fixture, harness, or test for the removed mode

## Acceptance criteria

- `Invoke-CodeQualityMetricsHistory.ps1` contains no working-tree read of
  `CodeQualityMetricsHistory.jsonl`: the only source-bytes path is the
  `Invoke-GitBytes` blob read at the supplied base commit.
- Running the script with `-Mode Contract -RepositoryRoot <worktree>
  -TipCommit <sha>` and no `-BaseCommit` fails on the missing mandatory
  parameter and writes no receipt.
- Running it with `-Mode Contract -RepositoryRoot <worktree> -BaseCommit
  <primary tip> -TipCommit <session tip>` still emits a single
  `broken-engine-code-quality-history-contract/v1` JSON object on stdout.
- `HistoryContract.md` no longer mentions a no-base or fixture-mode invocation.
- No file outside the two changed files is modified.

## Notes

Change Workflow tier: Tier 2 (scoped behaviour). Trigger: the root `AGENTS.md`
Tier-2 definition — one subsystem's tool behaviour. The change alters one
script's parameter contract and the reference document describing it; it touches
no determinism/CRC, wire/protocol, serialization or data layout, save/replay
compatibility, threading, or trust boundary, and the receipt bytes for every
production invocation are unchanged. A reviewer may escalate to Tier 3 if
implementation shows the landing caller must change after all, since that script
runs inside the landing flow and a defect there can block other sessions'
landings.

`/validate-skill` applies, because the change touches files inside the
`code-quality-metrics` skill package.
