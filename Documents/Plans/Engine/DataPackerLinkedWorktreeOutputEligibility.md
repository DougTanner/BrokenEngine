<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T14:15:22.912Z","dependsOn":[]} -->
# Reject a linked-worktree DataPacker run with a non-canonical output directory before exporting

## Context

In a linked (wrapper) worktree the tracked `ThirdParty` tree is a farm of
per-file symlinks into the primary checkout (observed: every entry of
`ThirdParty/bc7enc_rdo` is a symlink to
`.../Documents/BrokenEngine/ThirdParty/bc7enc_rdo/...`). DataPacker only escapes
those symlinks when `DiscoverLinkedWorktreeIdentity` succeeds, which redirects
`mThirdPartyDirectory` to the primary checkout
(`DataPacker/Source/FileManager.cpp:466-471`). That function returns `nullopt`
whenever the supplied output directory is not exactly
`<worktree>/Projects/<project>/Platforms/VisualStudio2026/Output/Data`
(`FileManager.cpp:240-245`), and that rejection is silent — no log, no error.

So a run using the documented three-argument CLI form
`<engine-data> <project-data> <output-data>` (`FileManager.cpp:370-380`) with any
other output directory keeps the worktree's symlinked `ThirdParty`. The whole
export then runs to completion and publishes, and only afterwards
`attribution::CopyThirdPartyLicenses()` (`DataPacker/Source/Main.cpp:673-674`)
walks that tree and hits the fail-closed reparse-point rule at
`DataPacker/Source/Attribution.cpp:56-66`, aborting the process with
`std::exception: Unsupported attribution reparse point: <worktree>\ThirdParty\bc7enc_rdo\bc7decomp.cpp`
and exit code 1.

Observed 2026-08-29 in worktree
`.claude\worktrees\BrokenEngine\33c70c73-8753-4ad6-8f40-1f18fa1c55bc` with
DataPacker Release|x64 built at baseline
`35f14a7e17347cf64488232fd5d2e2046259be85`, environment
`BT_DATAPACKER_FORBID_GAEA_EXPORT=1 BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1
BT_DATAPACKER_NONINTERACTIVE=1`. The zero-argument form, which derives the
canonical worktree output path, is unaffected and exits 0. Pre-existing:
the behavior comes from the eligibility check and the attribution rule quoted
above, neither of which the observing session touched (its only DataPacker edit
was staging code in `Main.cpp`).

The cost is two-fold: minutes to hours of export work are thrown away by a
failure that was already determined at startup, and the reported cause names a
`ThirdParty` file instead of the real cause, the ineligible output directory.

## Design

Recommendation: make the ineligibility explicit and immediate. In
`DiscoverLinkedWorktreeIdentity`, the branch that compares the expected and
supplied output paths (`FileManager.cpp:240-245`) is reached only after Git has
already confirmed a consistent linked worktree, so at that point the
configuration is known to be unsupported rather than merely "not a linked
worktree". Recommend throwing there instead of returning `nullopt`, with a
message naming the supplied output directory, the expected canonical one, and
the reason (a linked worktree must export into its own canonical output path so
`ThirdParty` and the output roots resolve to the validated primary sources).
The `rReject()` call in that branch is then redundant and should be dropped in
favor of the new throw.

Rationale for that recommendation over the alternative of redirecting
`mThirdPartyDirectory` to the primary checkout for any output directory: the
redirect would extend the validated linked-worktree contract (primary output
sources, reconciliation, copy-on-write materialization) to output roots that
were never validated against it, which is a larger and riskier change than the
failure it removes. Nothing in the repository invokes DataPacker this way —
`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:331-334` and
`.agents/scripts/Test-DataPackerMaterializeData.ps1:128-207` always pass the
canonical output path — so the recommended change only converts an always-failing
configuration into a fast, accurate failure.

Trade-off the implementer should confirm before committing to it: in a linked
worktree whose `ThirdParty` files are ordinary files rather than symlinks, a
non-canonical output run succeeds today and would become an explicit error. If
evidence turns up a real caller relying on that, surface it for re-planning
rather than widening scope.

## Critical files

- `DataPacker/Source/FileManager.cpp` — `DiscoverLinkedWorktreeIdentity`
  (`:195-247`) eligibility check; `InitializeWorktreeOutputs` (`:460-471`)
  ThirdParty redirect; CLI parsing and ThirdParty derivation (`:360-380`).
- `DataPacker/Source/Attribution.cpp` — `EnumerateLibraryFiles` (`:50-79`)
  fail-closed reparse rule that produces the late failure; read-only reference.
- `DataPacker/Source/AGENTS.md` — documents linked-worktree ThirdParty
  resolution and fail-closed reparse handling; update if the wording no longer
  matches.

## In scope

- The output-path eligibility branch of `DiscoverLinkedWorktreeIdentity` in
  `DataPacker/Source/FileManager.cpp`, and the diagnostic it produces.
- The matching sentence in `DataPacker/Source/AGENTS.md` if the change makes it
  inaccurate.

## Out of scope

- The fail-closed reparse rules in `DataPacker/Source/Attribution.cpp` and the
  other reparse checks in `FileManager.cpp`.
- How wrapper worktrees create `ThirdParty` symlinks.
- Output materialization, copy-on-write, cache, or export scheduling behavior.
- Any change to canonical-output runs, including the zero-argument form.

## Risk tier and invariants

Tier 2. Trigger: scoped tool behavior in one DataPacker unit — a check tightened
at an existing boundary, with no format, wire, determinism, or trust-boundary
change. Invariants to preserve: a linked worktree with the canonical output path
still resolves `ThirdParty` and both output roots to the validated primary
sources; a primary (non-linked) checkout is unaffected at any output path; the
existing reparse-point rejections keep failing closed.

## Acceptance criteria

- In a linked worktree, the three-argument form with a non-canonical output
  directory exits non-zero before any export work, with a message naming the
  supplied and expected output directories.
- The same invocation no longer reports
  `Unsupported attribution reparse point: <worktree>\ThirdParty\...`.
- The zero-argument form in a linked worktree still exits 0.
- `pwsh -NoProfile -File .agents/scripts/Test-DataPackerMaterializeData.ps1`
  passes unchanged.

## Notes

Recorded as a pre-existing residual observed while working on
`Documents/Plans/Engine/CrossVolumeExportPublication.md`; no source fix or build
was performed while recording it.
