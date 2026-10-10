# Explicit clang-tidy verification mode

## Authorization and when to use

Use this mode only when an approved plan explicitly requires a clang-tidy run. Outside this explicitly authorized mode, omit `-ClangTidy` so the build keeps `EnableClangTidyCodeAnalysis=false`.

## Retained protections

Retain every ordinary build protection the skill states: the rules for taking and releasing the short-lived operation lock, immutable prebuilt WorktreeCli, worktree provisioning and lifecycle validation, WorktreeCli target serialization, synchronous foreground execution, and, for a game build, data-mode selection with the authoritative data properties from [runtime-data-mode.md](runtime-data-mode.md). `-ClangTidy` changes only the analysis switches and the log verbosity and, on a full build, forces a rebuild. Do not invoke MSBuild or clang-tidy outside WorktreeCli.

Do not combine `-ClangTidy` with `-RunDataPacker`, although the script does not block it: when Local generation is authorized, run the generation build as an ordinary build first, then run each `-ClangTidy` build without `-RunDataPacker` against the existing Local output, so a forced rebuild never repeats an export.

## Invocation and coverage

`-ClangTidy` is accepted for Client and Server Debug and Release builds and for DataPacker, which builds Release only. Profile, every other target, and the combination with `-Prefast` block before any child process runs, under `parameter.clangtidy-invalid` — except that `-Prefast` invalid for the target or configuration on its own blocks first under `parameter.prefast-invalid`.

The mode passes `EnableClangTidyCodeAnalysis=true`, `EnableMicrosoftCodeAnalysis=false`, `RunCodeAnalysis=true`, and `/verbosity:normal`. Its Release builds take the `RunMsvcAnalysis`-off path that the "Microsoft code analysis" bullet of `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` `## Build Configuration` owns. The repository-root `.clang-tidy` sets an empty `WarningsAsErrors`, so a clang-tidy warning never fails the build. A parse error (`clang-diagnostic-error`) makes clang-tidy exit 1, which fails the build through `MSB6006` and leaves the project's remaining sources unanalyzed, so coverage requires a run with no parse errors.

- The `ClangTidy` target is incremental through its own tracking logs, so a full `-ClangTidy` build forces the rebuild itself and its result covers every compile item of the target. Budget for the longer runtime: this is a from-scratch rebuild of the whole target plus a clang-tidy pass over every compile item.
- Exception: a `-ClangTidy` run that also passes `-Files`, or any other incremental run, re-checks only what clang-tidy's own tracking finds out of date; `-Files` neither limits nor guarantees which files it re-checks, so the run carries no coverage guarantee. Only a full run establishes coverage.

```powershell
# BrokenEngineSandbox client and server Debug and Release, then DataPacker.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug -ClangTidy
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Release -ClangTidy
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Server -Configuration Debug -ClangTidy
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Server -Configuration Release -ClangTidy
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target DataPacker -ClangTidy
```

## Results

- Read diagnostics from the clang-tidy `warning:` lines in the retained log; they are plain clang-tidy output that MSBuild's end-of-build warning count does not include. Never read them from WorktreeCli's structured `diagnostics` array: clang-tidy check names do not match its letters-then-digits code shape, so those lines never enter it ([worker.md](worker.md) `diagnostics` owns the array's limits).
- Report "analysis executed" and "zero diagnostics" as separate facts. Execution evidence is the `ClangTidy` target and its clang-tidy invocation named per project in the retained log, which the normal verbosity prints. An empty or absent per-project `ClangTidy.log` proves neither: the task truncates it at start, the projects pass `--quiet` so a clean translation unit writes nothing, and a tracking-skipped run leaves it empty.
- A header diagnostic repeats once per translation unit that includes it, so de-duplicate by file, line, column, and check before counting.
- Report build status and `clang-diagnostic-error` parse errors as separate facts: a translation unit clang cannot parse reports `clang-diagnostic-error`, so search the retained log for it whatever the build status, and report its count with the first unparsable file.
