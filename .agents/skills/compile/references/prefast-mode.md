# Explicit Microsoft PREfast verification mode

Use this mode only when an approved plan explicitly requires Microsoft PREfast verification. Retain every ordinary game-build protection the skill states: the rules for taking and releasing the short-lived operation lock, immutable prebuilt WorktreeCli, worktree provisioning and lifecycle validation, WorktreeCli target serialization, synchronous foreground execution, and data-mode selection with the authoritative data properties from [runtime-data-mode.md](runtime-data-mode.md). `-Prefast` changes only the analysis switches and, on a full build, forces a rebuild, so those protections all still apply. Do not invoke MSBuild or `/analyze` outside WorktreeCli.

`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` owns the Microsoft code analysis explanation in its "Microsoft code analysis" bullet: the two Release paths selected by `RunCodeAnalysis`, the `EnablePREfast` gate, the rule set, `CodeAnalysisTreatWarningsAsErrors`, and the `CodeAnalysisNeverReportRuleErrors` prohibition. The facts this mode adds on top of it:

- Report "analysis executed" and "policy passed" as separate facts — a zero exit alone establishes only the second.
- `RunNativeCodeAnalysis` is an incremental target, so a green incremental run could otherwise mean "skipped as up-to-date". A full `-Prefast` build therefore forces the rebuild itself, and its returned result covers a full analysis pass — no separate rebuild or regeneration evidence is needed. Budget for the longer runtime: this is a from-scratch Release rebuild of the whole target, not an incremental one.
- Exception: a `-Prefast` run that also passes `-Files` stays incremental for the selected files and carries no rebuild guarantee, so it does not establish that analysis ran across the target.

`-Prefast` forces Clang-Tidy off and Microsoft code analysis on, and it is accepted for Client and Server Release builds only. Do not override the projects' warnings-as-errors settings, the rule set, or `CodeAnalysisTreatWarningsAsErrors`, and never pass `CodeAnalysisNeverReportRuleErrors` — it disables error promotion silently:

```powershell
# BrokenEngineSandbox client Release PREfast, then server Release PREfast.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Release -Prefast
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Server -Configuration Release -Prefast
```

`EnableMicrosoftCodeAnalysis=true`, which `-Prefast` passes, is an extra safety measure here: the toolchain forces analysis off only on an explicit `false`, and the Release configurations set it nowhere, so passing it changes nothing today. Keep it so an upstream default change cannot silently disable the mode.

A policy failure surfaces as a nonzero MSBuild exit after the link step, with the executable already produced — analysis runs through `AfterBuildLinkTargets`. Existing binaries after a failed run are expected, not a partial success. A failing run also does not write `*.lastcodeanalysissucceeded`, so the failure correctly re-reports on the next build until it is fixed. Report the matched diagnostics from the structured `diagnostics` array, noting `diagnosticsTruncated: true` and pointing at the retained log when the cap elides the rest.

Outside this explicitly authorized mode, omit `-Prefast` so the build keeps `EnableClangTidyCodeAnalysis=false` and `RunCodeAnalysis=false`; never infer PREfast authorization from a routine compile, rebuild, or link-error check.
