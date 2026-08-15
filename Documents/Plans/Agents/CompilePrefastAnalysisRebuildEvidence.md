<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:47:59.573Z","dependsOn":[]} -->
# Fix: compile PREfast mode — a green build can report analysis that never ran

## Context

`.agents/skills/compile/references/prefast-mode.md:8` already states the hazard
in its own words: "`RunNativeCodeAnalysis` is an incremental target, so a green
incremental run can mean 'skipped as up-to-date'; require a rebuild, or positive
evidence that this run regenerated the merged `.nativecodeanalysis.xml`, before
reporting that analysis ran." Nothing in the skill supplies either one.

The documented PREfast invocations at
`.agents/skills/compile/references/prefast-mode.md:14-15` are ordinary
incremental builds, and `Invoke-CompileBuild.ps1` never adds a rebuild: the
`-Prefast` switch only swaps analysis properties
(`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:350-356` returns
`EnableClangTidyCodeAnalysis=false`, `EnableMicrosoftCodeAnalysis=true`,
`RunCodeAnalysis=true`), and the argument assembly at `:358-379` passes no
`/t:Rebuild` and no equivalent. `-Prefast` is validated only for target and
configuration (`:407-409`). The skill also emits no signal a caller could use as
the alternative "positive evidence" — no merged `.nativecodeanalysis.xml`
regeneration check exists anywhere in the compile skill.

The result is a silent false pass: on an up-to-date Release tree the documented
PREfast run exits `0` with MSBuild skipping `RunNativeCodeAnalysis`, and the
caller reports PREfast verification as executed when no source was analyzed.
Because analysis-clean state is also recorded by `*.lastcodeanalysissucceeded`,
the stale-green outcome is the normal case for any second run.

This gap is pre-existing, not introduced by the change that surfaced it. The
baseline `git show
479a8a6d8aea5df9f15d6fd796ffaad8ba609733:.agents/skills/compile/references/prefast-mode.md`
carries the same warning and the same rebuild-free commands (then written as
direct `WorktreeCli build` invocations); the current `-Prefast` parameter merely
inherits it. It is outside the boundary of
`Documents/Plans/Agents/CompileAbsoluteTargetPathResolution.md`, whose scope is
the documented full-build target invocations and `BuildCommand.cpp` target path
handling.

## Design

Make a `-Prefast` run prove that analysis actually ran, rather than leaving the
proof to the caller.

`Invoke-CompileBuild.ps1` forces a rebuild on the `-Prefast` path: when
`-Prefast` is set, the WorktreeCli build argument list gains `/t:Rebuild` so
`RunNativeCodeAnalysis` cannot be skipped as up-to-date. Only the `-Prefast`
path changes; ordinary builds stay incremental. All other `-Prefast` behavior is
unchanged: the same analysis properties, the same Client/Server Release
restriction, the same data-mode, oracle, lock, and serialization protections,
and the same verbatim WorktreeCli result envelope on stdout.

`references/prefast-mode.md` is updated to match: the bullet that today demands
"a rebuild, or positive evidence" becomes a statement that `-Prefast` itself
forces the rebuild, so a returned result covers a full analysis pass, while
keeping the separate facts that a zero exit establishes "policy passed" and not,
by itself, anything else. The longer runtime of a forced Release rebuild is
noted so a caller budgets for it.

If implementation shows `/t:Rebuild` cannot be applied through the WorktreeCli
build verb without changing WorktreeCli itself, surface that for re-planning
rather than expanding scope into `Tools/WorktreeCli`, which is
build/bootstrap-coordination surface.

## Critical files

- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` — `-Prefast`
  parameter (`:24`), `Get-AnalysisArguments` (`:350-356`),
  `Invoke-WorktreeCliBuild` argument assembly (`:358-379`), and the `-Prefast`
  parameter validation (`:407-409`).
- `.agents/skills/compile/references/prefast-mode.md` — the incremental-target
  warning (`:8`), the documented invocations (`:12-16`), and the surrounding
  reporting rules.

## In scope

- `Invoke-CompileBuild.ps1`: adding the forced rebuild to the `-Prefast` build
  invocation only, in `Get-AnalysisArguments` or `Invoke-WorktreeCliBuild`,
  whichever keeps the change smallest.
- `references/prefast-mode.md`: the analysis-coverage bullet and any invocation
  or reporting text that the forced rebuild makes inaccurate.

## Out of scope

- Ordinary non-`-Prefast` builds, which stay incremental.
- `Tools/WorktreeCli` sources, the build verb's own contract, and the
  `broken-engine-build-result/v1` envelope shape.
- The rule set, `EnablePREfast`, `CodeAnalysisTreatWarningsAsErrors`, and the
  `CodeAnalysisNeverReportRuleErrors` prohibition, all owned by
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`.
- Data-mode selection, oracle receipts, lock handling, and every other compile
  skill protection.
- Clang-Tidy enablement and any other compile skill workflow section.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior). Escalate to Tier 3 if the smallest fix
must change `Tools/WorktreeCli`, which is build/bootstrap coordination that can
block other sessions. No determinism/CRC, serialization, `.pack`, replay, wire,
shader, or runtime allocation surface is exposed. Preserve the verbatim
WorktreeCli result envelope, the Client/Server Release-only `-Prefast`
restriction, and the prohibition on invoking MSBuild or `/analyze` outside
WorktreeCli.

## Acceptance criteria

- A `-Prefast` Client Release run against an already up-to-date tree
  recompiles and re-analyzes rather than reporting green with
  `RunNativeCodeAnalysis` skipped as up-to-date.
- A non-`-Prefast` build of the same target is still incremental — no rebuild is
  forced when `-Prefast` is absent.
- `references/prefast-mode.md` no longer instructs the caller to supply a
  rebuild or regeneration evidence that the skill does not provide, and still
  keeps "analysis executed" and "policy passed" as separate reported facts.
- `/validate-skill` passes for the compile skill, and WorktreeCli
  `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the pair (compile skill `-Prefast` path, a green run can
mean analysis was skipped as up-to-date). It is distinct from
`Documents/Plans/Agents/WorktreeCliBuildTargetNormalizedIdentity.md` (wrong
normalized target identity in a successful result) and from
`Documents/Plans/Agents/CompileSharedDataVersionDrift.md` (Shared runtime
data/source version mismatch).
