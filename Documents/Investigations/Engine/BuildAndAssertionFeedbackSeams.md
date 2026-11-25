# Build and Assertion Feedback Seams

Audit of Broken Engine's build, static-check, and runtime-assertion feedback
loops for faster, more localized, and more informative agent iteration.

## What Already Works Well

- **Structured, serialized builds**: `Tools/WorktreeCli/BuildCommand.cpp` emits
  one `broken-engine-build-result/v1` JSON object with parsed, deduplicated
  compiler/linker diagnostics, per-target locking, private `_MSPDBSRV_ENDPOINT_`
  isolation, retained logs under `Temp/AgentBuildLogs/`, and `--files` object
  invalidation for selective `.cpp` rebuilds. Prebuilt `ThirdParty` and
  `AgentTools`, prebuild-seeded `DataPacker`/`ModelViewer`
  (`.agents/scripts/Build-WorktreeTarget.ps1`), and `DataBuildMode=Shared`
  (`Engine/Platforms/VisualStudio2026/Engine.props`) avoid redundant worktree
  builds and asset repacking.
- **Fast diff-scoped static checks**: `.agents/scripts/Invoke-StaticChecks.ps1`
  runs seven read-only checks in one pass with JSON output.
  `Invoke-CollectionLayoutCheck.ps1` audits SOA `Collection<T>` headers without
  compiling, and `Invoke-IncludeOrderCheck.ps1` checks and fixes include order.
- **Headless runtime verification**: `Tools/AgentHarness/` and
  `Engine/Source/Agent/` expose a loopback JSON channel with structured state
  queries, synthetic UI/input, replay determinism checks, live `set_log_level`,
  the wrapping `gpcLogAgentLines` ring buffer (`Common/Log/Log.h`),
  `LogDifference` desync diffs, and debugger-free `DEBUG_BREAK()` site logging
  (`Common/ErrorUtils.cpp`).

## Gaps and Options

### 1. Value-Printing Assertions and `[[noreturn]]` Failure Path

Across `Common/`, `Engine/Source/`, `Projects/BrokenEngineSandbox/Source/`,
`DataPacker/Source/`, and `Projects/ModelViewer/Source/`, there are 538
`ASSERT(...)` calls; 334 (62%) are binary comparisons (`==`, `!=`, `<`, `<=`,
`>`, `>=`).

Currently `Common/ErrorUtils.h` defines `ASSERT(a)` to pass only `(bAssertMacro,
#a)` to `common::Assert(...)`, which logs `"Assert failed: \"{}\" at {}:{} in
{}"` without operand values or an invariant label. When a headless harness run
trips an assertion, the agent must add temporary `LOG(kTemp, ...)` code,
rebuild, and relaunch just to inspect the failing values.

In addition, `common::Assert(bool, ...)` is not `[[noreturn]]` because it
re-checks `if (!bCondition)` out of line. `Engine.props` (line 44) explicitly
notes that this prevents clang-analyzer from seeing assertion postconditions and
forced disabling `clang-analyzer-security.ArrayBound`.

Options to decide:

- **Option 1: `[[noreturn]]` failure helper + comparison/message macros** —
  Split `common::Assert` into `[[noreturn]] void AssertFailed(...)` called from
  the macro's `if (!bAssertMacro) [[unlikely]]` branch (preserving
  `_Analysis_assume_`), and add `ASSERT_MSG(cond, fmt, ...)` plus comparison
  macros (`ASSERT_EQ`, `ASSERT_NE`, `ASSERT_LT`, `ASSERT_LE`, `ASSERT_GT`,
  `ASSERT_GE`) that evaluate each operand once and log `actual` vs. `expected`
  via `std::format`. Migrate high-value call sites opportunistically.
- **Option 2: `[[noreturn]]` failure helper + `ASSERT_MSG` only** — Keep a
  single boolean `ASSERT` macro family without six comparison macros, adding
  only `ASSERT_MSG(cond, fmt, ...)` for rich context where needed.

Open questions:

- Which macro surface fits `Documents/C++StyleGuide.txt` (Option 1 vs. Option
  2)?
- Does making `AssertFailed` `[[noreturn]]` allow re-enabling
  `clang-analyzer-security.ArrayBound` in `Engine.props` without noise?

### 2. PCH Rebuild Blast Radius on High-Churn Headers

`Engine.props` force-includes `Pch.h` into all 224 Sandbox Client `.cpp` files
(and all Server `.cpp` files). `Projects/BrokenEngineSandbox/Source/Pch.h`
aggregates `ExternalHeaders.h`, `Common.h` (34 headers), `ShaderLayouts.h`,
`Frame.h` (all collections), `Engine.h` (47+ headers), `GameBase.h`, and
`GameMessages.h`.

Over the last 200 commits, most of the top 25 most-edited headers live inside
this PCH: `NetworkProtocol.h` (31), `Omnis.h` (28), `ShaderGlobalLayout.h`
(28), `Render.h` (25), `GameBase.h` (25), `Battleships.h` (24), `Fighters.h`
(22), `Frame.h` (20), `IslandTerrain.h` (20), `DataFile.h` (20), `Graphics.h`
(17), `CameraBase.h` (17), and `ImGuiManager.h` (17). Any edit to these headers
invalidates the PCH and triggers a full recompile of both Client and Server.

`Engine/Source/AGENTS.md` (`## Hub Conventions`, `Internals:`) already endorses
moving heavy private state in `Engine.h`-aggregated headers behind
`std::unique_ptr` (precedent: `FileManager`/`PackChunks`), while keeping SOA
collection storage flat.

Next steps before a Plan:

- Measure actual PCH-invalidation rebuild time (Client + Server Debug) and
  consumer fan-out for the top-churn `Engine.h` headers (`Render.h`,
  `Graphics.h`, `ImGuiManager.h`, `IslandTerrain.h`, `NetworkProtocol.h`,
  `CameraBase.h`).
- Identify which headers can either move private state behind `std::unique_ptr`
  or move out of `Engine.h` altogether when only a few `.cpp` files include
  them, without adding per-access indirection to hot loops.

### 3. Fast Compile-Only / Debug Incremental-Link Loop

`WorktreeCli build --files` invalidates selected `.obj` files and invokes a full
MSBuild build including link. Because `Engine.props` sets
`LinkIncremental=false` for all configurations, even a single-file Debug `.cpp`
compile check pays a full non-incremental link.

Next steps before a Plan:

- Measure Debug link time vs. single-file `ClCompile` time on Client and Server.
- Decide between (a) enabling `LinkIncremental=true` in Debug only (keeping
  Profile/Release `false` with LTCG), (b) adding a `--compile-only` mode
  (`/t:ClCompile` with `/p:SelectedFiles=...`) to `WorktreeCli build` for
  syntax/type checks that do not need a runnable binary, or both.
