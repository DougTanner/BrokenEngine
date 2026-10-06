<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T18:45:57.442Z","dependsOn":[]} -->
# Fix the client Release PREfast diagnostics

## Context

A full BrokenEngineSandbox client `Release|x64` PREfast build (`/compile` `-Prefast`: `RunCodeAnalysis=true`, `/t:Rebuild`) at primary `2b4cb8d3` exits 1 with 24 error-severity C26xxx Core Guidelines diagnostics. `BrokenEngineAnalysis.ruleset` is `IncludeAll` with no per-code exceptions, and `CodeAnalysisTreatWarningsAsErrors=true` promotes every reported rule to an error (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`, "Microsoft code analysis" bullet). Agent builds outside PREfast mode pass `RunCodeAnalysis=false`, which never applies the rule set, so these accumulated unseen. Effect: an approved plan that requires PREfast verification (`.agents/skills/compile/references/prefast-mode.md`) cannot get a passing PREfast build.

None of the 24 sites is in the session that observed them; every cited line exists unchanged at the primary tip. Each has a confirmed local cause:

- C26473 (type.1), same-type pointer cast — x5. `ScopedWorkbufferAllocation<T>::mpData` is already `T` (`Common/Workbuffer.h:261`), and each site `static_cast`s it to that same `T`, as returned by `PushBuffer<T*>`:
  - `Engine/Source/File/FileManager.cpp:336` (`std::byte*`)
  - `Engine/Source/Frame/Collections/Collection.h:208` (`int64_t*`)
  - `Engine/Source/Graphics/AnimationData.cpp:326` (`std::byte*`)
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:591` (`XMFLOAT4*`)
  - `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:494` (`std::byte*`)
- C26459 (stl.1), `std::copy` with a raw-pointer output iterator — x8. Each is `std::copy(std::begin(a), std::end(a), std::begin(b))` over C arrays:
  - `Common/Crc.h:15` (`FixedString` constexpr constructor)
  - `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:384`
  - `Engine/Source/Graphics/Managers/TextureManager.cpp:195`
  - `Engine/Source/Input/RawInputManager.cpp:148`
  - `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:391`, `:411`, `:461`, `:492`
- C26497 (f.4), function can be `constexpr` — x9:
  - `engine::PointInPolygon` (`Engine/Source/Frame/NavBuild.cpp:281`)
  - `game::PlayersPostRender::IsBlasterFireTimeInRange` and `IsNavigationDelayInRange` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:198`, `:204`)
  - `engine::DisplayLength` (`Engine/Source/Agent/AgentUiRegistry.cpp:27`)
  - `engine::CopyStringParameter` (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:816`)
  - `engine::SelectSampleCount` (`Engine/Source/Graphics/Managers/InstanceManager.cpp:92`)
  - `common::BuildWorldHull` and `common::IsPolygonCcw` (`Common/Math/ConvexHull.h:21`, `:103`)
  - `engine::IsValidGraphicsQualityLevel` (`Engine/Source/Ui/GraphicsSettings.cpp:101`)
- C26492 (type.3), `const_cast` — x1. `engine::PlotSmoothed` (`Engine/Source/Profile/NetworkGraphs.cpp:30`) casts away const on `const common::Smoothed<int64_t>&` to pass ImPlot's `void*` getter data; `SmoothedGetter` only reads through it.
- C26445 (gsl.view), view bound by reference — x1. `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:500` iterates `TweaksSliderMap::Get()` with `const auto& [key, pWrapper]`, binding the `std::string_view` key by reference.

## Design

Recommended fix for each class. Every edit is behavior-preserving; none changes computed values, layout, or CRC identity.

- C26473: drop the redundant `static_cast` and use `mpData` directly.
- C26459: replace each `std::copy(std::begin(a), std::end(a), std::begin(b))` with `std::ranges::copy(a, b)` or `std::ranges::copy(a, std::begin(b))`, whichever the analyzer accepts. Keep `Crc.h` `FixedString` usable in constant evaluation. Recommended because the ranges form passes array bounds instead of a raw output pointer. If the analyzer still flags the ranges form, wrap the destination in `std::span` and use its iterator, as the diagnostic suggests.
- C26497: add `constexpr` to each named function, removing `static`/`inline` only where `constexpr` makes it redundant. If a function cannot be `constexpr` because it calls a non-constexpr function, such as a DirectXMath or `common::` deterministic-math call, restructure nothing. Record that site in this Plan's completion notes and suppress that one code at that one function with `[[gsl::suppress("f.4")]]` (MSVC's attribute form). Simulation functions such as `PointInPolygon` and `BuildWorldHull` still run at runtime under `/fp:strict`; `constexpr` adds compile-time evaluation only in constant contexts.
- C26492: avoid the cast. Recommended: take `PlotSmoothed`'s parameter as non-const `common::Smoothed<int64_t>&` if every caller passes a mutable object, which the implementer confirms at the call sites. Otherwise apply `[[gsl::suppress("type.3")]]` on `PlotSmoothed`, stating that ImPlot's getter API takes `void*` and the getter only reads.
- C26445: bind the structured binding by value (`for (const auto [key, pWrapper] : ...)`); the element is a `std::string_view` and a pointer.

Never change the rule set, `CodeAnalysisTreatWarningsAsErrors`, or `EnablePREfast`, and never pass `CodeAnalysisNeverReportRuleErrors` (`prefast-mode.md`).

Change Workflow tier: Tier 2. Trigger: public-header declaration edits in independently owned layers (`constexpr` on `Common/Math/ConvexHull.h` and `Players.h` functions; the `Common/Crc.h` `FixedString` constructor that produces compile-time CRC input). All of them are behavior-preserving, so no determinism/CRC, serialization, wire, threading, or trust surface changes. That keeps it below Tier 3. A reviewer escalates if any edit changes a computed value.

## Critical files

- `Common/Crc.h`, `Common/Math/ConvexHull.h`
- `Engine/Source/File/FileManager.cpp`, `Engine/Source/Frame/Collections/Collection.h`, `Engine/Source/Frame/NavBuild.cpp`
- `Engine/Source/Agent/AgentUiRegistry.cpp`, `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`
- `Engine/Source/Graphics/Managers/InstanceManager.cpp`, `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`, `Engine/Source/Graphics/Managers/TextureManager.cpp`, `Engine/Source/Graphics/AnimationData.cpp`
- `Engine/Source/Input/RawInputManager.cpp`, `Engine/Source/Profile/NetworkGraphs.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp`, `Engine/Source/Ui/GraphicsSettings.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp`, `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`

## In scope

- Exactly the 24 cited lines or functions above, plus the minimum adjacent edit each fix needs: a caller's argument when `PlotSmoothed`'s parameter changes, or a `static`/`inline` keyword made redundant by `constexpr`.

## Out of scope

- Any other PREfast, Clang-Tidy, or style diagnostic, including ones server Release `-Prefast` reports in server-only code; record those as a separate follow-up.
- Changes to `BrokenEngineAnalysis.ruleset`, the vcxproj analysis properties, or the `/compile` PREfast mode.
- Refactoring the surrounding functions, `Workbuffer`, `TweaksSliderMap`, or ImPlot usage beyond the cited sites.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Release -Prefast` exits 0 with no diagnostics. Report "analysis executed" (full rebuild) and "policy passed" separately, per `prefast-mode.md`.
- Ordinary client and server Debug and Release builds pass.
- Behavior is unchanged: replay determinism through `/agent-harness` matches its pre-change result, because `Crc.h`, `ConvexHull.h`, `NavBuild.cpp`, `Collection.h`, `Spaceships.cpp`, and `Frame.cpp` sit on CRC-fed or simulation paths.

## Notes

- Evidence: the client Release PREfast envelope `Temp/AgentBuildEnvelopes/compile-20261006T183713411Z-8776.md` (24 diagnostics, `diagnosticsTruncated: false`).
- Server Release `-Prefast` was not run when this was observed. Several cited files are shared with the server (`Common/`, `Collection.h`, `NavBuild.cpp`, `FileManager.cpp`, `Players.h`, `Spaceships.cpp`, `Frame.cpp`). The fixes here clear those shared sites for both targets; a server-only remainder is out of scope.
