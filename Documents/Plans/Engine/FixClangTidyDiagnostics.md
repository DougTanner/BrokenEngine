<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-10T00:31:35.758Z","dependsOn":[]} -->
# Run clang-tidy over the repository's C++ and fix what it reports

## Context

The user asked for a clang-tidy run over the repository's own C++, with the existing issues it reports fixed, because agents have not seen its output for a while.

Nothing an agent runs reports clang-tidy diagnostics today:

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:26` — agent builds via `/compile` force-disable clang-tidy (about 1,800 CPU-seconds per client build), "so tidy diagnostics appear only in the IDE".
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:143-147` (`Get-AnalysisArguments`) passes `/p:EnableClangTidyCodeAnalysis=false` on every path, `-Prefast` included; `.agents/skills/compile/references/worker.md:324` states the same rule.
- `.agents/skills/compile/references/prefast-mode.md:9` forbids invoking MSBuild or `/analyze` outside WorktreeCli, so a raw MSBuild `ClangTidy` run is not an agent route.
- The other documented routes do not work for an agent. The IDE route (`.clang-tidy:3-8`) needs a person at Visual Studio. The bare CLI line `clang-tidy <file.cpp> -- -std=c++23` (`.clang-tidy:11-12`) has none of the projects' defines, include directories, or `Common/ClangTidyShims`, so it cannot parse the engine sources.

Where the IDE does run it: `EnableClangTidyCodeAnalysis` is true in client Debug and Release (`BrokenEngineSandbox.vcxproj:81,113`; Profile off, `:94`), in every server configuration (`BrokenEngineSandboxServer.vcxproj:81,93,111`), and in DataPacker Debug and Release (`DataPacker.vcxproj:154,164`). The toolchain runs it during a build only when `RunCppAnalysis` is also true (`Microsoft.Cpp.Analysis.targets:19` in the VS 2026 install, `MSBuild/Microsoft/VC/v180`). Only the game Release configurations set `RunCodeAnalysis` true (client `:111`, server `:109`). Client Debug (`:82`) and both DataPacker configurations (`:155,163`) set it false, so they analyze only through the IDE's explicit Run Code Analysis command. The run's check set is `.clang-tidy` `Checks` (`.clang-tidy:28-45`) plus the `clang-analyzer-*` suite that each vcxproj's `<ClangTidyChecks>` appends (client `:117`, server `:115`, DataPacker `:168`). Its header filter covers `Common`, `Engine`, `DataPacker`, and `Projects` and excludes `ThirdParty` (`.clang-tidy:51-52`). No project compiles a `ThirdParty` source.

The bundled tool is `VC/Tools/Llvm/x64/bin/clang-tidy.exe`, LLVM 22.1.3. Run from the worktree root, its `--list-checks` lists 16 enabled checks. `llvm-prefer-defined` was removed from `.clang-tidy` because clang-tidy has no such check; style rule 58 stays checked by the scanner's `style-rule-58` kind (`.agents/scripts/Find-SessionCandidates.ps1`).

The `.clang-tidy` header (`.clang-tidy:7-8`) already expects this work: "pre-existing code may produce warnings until a follow-up cleanup pass lands."

## Design

Recommended route: give `/compile` a `-ClangTidy` switch that mirrors `-Prefast`. No agent route exists. `-Prefast` is the existing pattern for an opt-in analysis mode that keeps WorktreeCli serialization, the data properties, and the retained log. A parallel switch reuses that mechanism instead of adding a new script or harness. The alternative is to have the user run IDE builds and hand over the Error List. The author rejects it because every re-check after a fix would need the user.

1. **`-ClangTidy` mode.** In `Invoke-CompileBuild.ps1`, add the switch beside `-Prefast`:
   - Accept it for Client and Server in Debug or Release, and for DataPacker (Release only).
   - Reject it with `-Prefast`, in the same parameter-contract style as `parameter.prefast-invalid` (`:202-203`).
   - `Get-AnalysisArguments` passes `EnableClangTidyCodeAnalysis=true` and `RunCodeAnalysis=true`, so `RunClangCppAnalysis` holds. It passes `EnableMicrosoftCodeAnalysis=false`, so the rule-set path stays off.
   - A full run (no `-Files`) adds `/t:Rebuild`, as `-Prefast` does (`:167-171`), because the `ClangTidy` task is tracked incrementally.
   - A `-Files` run stays incremental and only re-checks the selected files.

   Document the mode in a `references/clang-tidy-mode.md` parallel to `prefast-mode.md` and add the matching authorization bullet to `SKILL.md`. Correct the sentences the mode makes false: `worker.md:324`, `prefast-mode.md:5`, and `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:26`. clang-tidy warnings do not fail the build (`WarningsAsErrors: ''`, `.clang-tidy:47`). The diagnostics are the `[<check-name>]` warning lines in each build's retained log. Following `prefast-mode.md` `## Invocation and coverage`, report "analysis executed" separately from "zero diagnostics": the log must show the `ClangTidy` target ran over the project's sources.
2. **Baseline run.** Run full `-ClangTidy` builds of Client Debug, Client Release, Server Debug, Server Release, and DataPacker Release. Debug and Release together cover `BT_DEBUG`-only and Release-only code. Profile is omitted. The client disables clang-tidy there (`:94`). The server enables it (`BrokenEngineSandboxServer.vcxproj:93`), but its Profile build differs from Release only by `BT_PROFILE` in place of `BT_RELEASE` (`:181,250`), and `BT_PROFILE`-only regions are left for a later run. Tabulate the diagnostics by check and top-level directory, de-duplicating headers reported by several translation units. If the count is too large to fix and review in one session, the author recommends stopping after landing step 1 and the tabulation, and proposing per-area follow-up Plans instead of a partial sweep.
3. **Fix.** Fix each diagnostic by hand in the form `Documents/C++StyleGuide.txt` prescribes, never by applying clang-tidy's `--fix` output. Known divergences:
   - `misc-unused-parameters`: use `[[maybe_unused]]` (rule 39, `:202`), not a commented-out name.
   - `performance-unnecessary-value-param` on a string: use `std::string_view` (rule 40, `:204`), not `const std::string&`.
   - `cppcoreguidelines-pro-type-cstyle-cast`: use a C++-style cast (rule 11, `:93`).
   - `readability-implicit-bool-conversion`: write an explicit comparison (rule 50, `:246`).

   Every fix preserves meaning. A diagnostic whose only fix would change behavior — a true `bugprone-use-after-move`, `bugprone-integer-division`, or analyzer defect — is recorded for its own follow-up Plan, not fixed here.
4. **Record what is not fixed.**
   - A per-site false positive gets the repository's existing suppression form, `// NOLINTNEXTLINE(<check>) — <reason>` (for example, `Engine/Source/Frame/Collections/CollectionController.h:67`).
   - A check that conflicts with the style guide as a whole is listed with its rule and count and surfaced to the user rather than silenced or reconfigured.
   - A diagnostic in a DataPacker-generated `$(GameDataDirectory)` header is listed, because its fix belongs in the DataPacker emitter and needs Local data generation.
5. **Re-run** the step 2 matrix and record the result.

## Critical files

- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`, `.agents/skills/compile/SKILL.md`, `.agents/skills/compile/references/worker.md`, `.agents/skills/compile/references/prefast-mode.md`, new `.agents/skills/compile/references/clang-tidy-mode.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
- The C++ sources and headers under `Common/`, `Engine/`, `DataPacker/`, and `Projects/` that the baseline run reports

## In scope

- The `-ClangTidy` switch, its validation and analysis arguments in `Invoke-CompileBuild.ps1`, and the four compile-skill documents above
- The `## Build Configuration` Clang-Tidy bullet in `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
- Meaning-preserving fixes, and `NOLINTNEXTLINE` suppressions with a reason, at the exact sites the run reports in `Common/`, `Engine/`, `DataPacker/`, and `Projects/` source and header files
- `AGENTS.md` updates that a fix makes necessary

## Out of scope

- `ThirdParty/`, and any change to `.clang-tidy` or to a vcxproj's `<ClangTidyChecks>`, `EnableClangTidyCodeAnalysis`, or `RunCodeAnalysis` and any check whose conflict with the style guide is surfaced instead
- `Tools/`: AgentHarness and WorktreeCli enable no clang-tidy, and the header filter omits `Tools`, so adding them would be a configuration change, not a run of the existing one
- Behavior-changing fixes, and DataPacker emitter changes for generated headers; these become follow-up Plans
- Changes to the default `/compile` path, `-Prefast`, or Microsoft code analysis; refactors or style cleanup beyond the reported sites

## Risk tier and invariants

Expected Tier 3. Trigger: the change spans independently owned subsystems — `Common`, `Engine`, `DataPacker`, the game under `Projects/`, and the shared `/compile` build entry point that every session's builds pass through. Invariants:

- Every C++ fix is meaning-preserving, so PostRender state, CRC, wire, save, replay, and `.pack` formats are unchanged and no `kiVersion` changes.
- Signature changes from `performance-unnecessary-value-param` must keep the call semantics, including aliasing of the moved-from or referenced argument.
- The default `/compile` arguments stay byte-identical.

## Acceptance criteria

- `/compile` with `-ClangTidy` runs clang-tidy over each project: the retained log shows the `ClangTidy` target analyzed the project's sources. A default `/compile` invocation's WorktreeCli build arguments are unchanged.
- The final full `-ClangTidy` runs of Client Debug, Client Release, Server Debug, Server Release, and DataPacker Release report no clang-tidy diagnostic for the configured checks and no `clang-diagnostic-error`. Every exception carries a `NOLINTNEXTLINE` reason or appears in the landing summary's list of surfaced style-guide conflicts, generated-header diagnostics, and behavior-changing findings.
- Ordinary `/compile` Client and Server Debug builds succeed.
- When any fix touched simulation code, an `/agent-harness` replay determinism check passes.

## Notes

- `## Sweep` is deliberately absent. Every `/sweep` type finds work by hand-reading each unit file (`.agents/skills/sweep/references/types/style-guide.md:23`, `cpp-adoption.md:8`). Here the work comes from tool diagnostics that only a `/compile -ClangTidy` run produces, and each diagnostic already names its site, so per-file Codex find runs would repeat the tool.
- A check added to `.clang-tidy` before this Plan runs, such as `readability-static-accessed-through-instance` for style rule 72, is part of the configured set at run time and needs no dependency edge.
- Origin: the user's direction in a Claude session on branch `claude/82237093-c04a-4789-8e8b-d5b6fca7a221`.
