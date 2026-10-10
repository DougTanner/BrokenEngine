<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-10T00:31:35.758Z","dependsOn":[]} -->
# Fix the clang-tidy diagnostics in the repository's C++

## Context

The user asked for a clang-tidy run over the repository's own C++, with the existing issues it reports fixed. A first session on branch `claude/88277bb9-e7bd-4929-a054-141f843a8de5` built the agent route, made the sources parse under clang, and measured the Debug baseline. This Plan carries only the remaining work: fixing the reported warnings.

What already exists:

- `/compile -ClangTidy`. `.agents/skills/compile/references/clang-tidy-mode.md` owns the accepted targets (Client and Server Debug and Release, DataPacker Release), the arguments, coverage, and how to read results. The mode passes `/verbosity:normal` (the user's choice; that mode only), and a full run forces `/t:Rebuild`. Ordinary `/compile` builds still force clang-tidy off (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` `## Build Configuration`).
- `Common/ExternalHeaders.h:8-12`: a `__clang__`-guarded empty `#define __restrict`. clang keeps `__restrict` in pointer type identity, so without it the static_asserts at `Engine/Source/Frame/Collections/CollectionMemory.h:232,236` and a `std::span` construction in `Engine/Source/Frame/Collections/Collection.h` failed to parse. The user chose this location over per-site code edits and over a `.clang-tidy` compiler argument. MSVC never sees the define.
- `Engine/Source/Network/Server/Server.cpp:781-782`: a narrowing initialization clang rejected, now `static_cast<uint8_t>(cond ? 1 : 0)`; wire bytes unchanged.

Measured full `-ClangTidy` runs (0 parse errors, every compile item analyzed):

| Configuration | Time | Sources analyzed | Warnings (de-duplicated) |
|---|---|---|---|
| Client Debug | 21m09s | 227/227 | 29 |
| Server Debug | 11m09s | 109/109 | 14 |
| DataPacker Release | 4m44s | 39/39 | 22 |

Client Release and Server Release were not run; they can add Release-only findings or a parse error.

Run behavior the first session observed:

- A parse error (`clang-diagnostic-error`) fails the build through `MSB6006` and stops analysis of the project's remaining sources.
- The MSBuild end summary can report `0 Warning(s)` while clang-tidy warning lines are present in the log. Tabulate from the warning lines, de-duplicated by file:line:col:check and across translation units for headers.
- The `.clang-tidy` header filter (`.clang-tidy:51-52`) does not suppress `clang-analyzer` path diagnostics reported inside ThirdParty headers reached from repository code.

Baseline: 47 unique sites across the three runs (the `Common/WindowsUtils.cpp` sites appear in all three; `PackChunks.cpp:338,354` and `NetworkSimulation.h` in client and server):

- `readability-implicit-bool-conversion` (32): `Common/WindowsUtils.cpp:147,156,164,169,183,193,207`; `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:408,512`; `Engine/Source/Agent/AgentInput.cpp:467,471,475,479,483`; `Engine/Source/File/PackChunks.cpp:338,354,942`; `Engine/Source/Graphics/AnimationData.cpp:323,358`; `Engine/Source/Input/Input.cpp:119`; `Engine/Source/Main.cpp:471` (two, server); `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:245` (three); `DataPacker/Source/FileManager.cpp:154,258,329,514,557`; `DataPacker/Source/InputFingerprint.cpp:414,415`.
- `bugprone-unchecked-optional-access` (4): `Engine/Source/Network/Client/ReconcileReplay.cpp:223,232,240,242`.
- `clang-analyzer-deadcode.DeadStores` (4): `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:19`; `Engine/Source/Network/NetworkSimulation.h:166,167`; `Engine/Source/Network/Server/ServerBroadcaster.cpp:149`.
- `clang-analyzer-security.ArrayBound` (2): `DataPacker/Source/ExportJobs/ExportShader.cpp:46`; `DataPacker/Source/ExportJobs/ExportTexture.cpp:269`.
- `modernize-use-override` (1): `Engine/Source/Audio/StreamingVoice.h:47`.
- `performance-for-range-copy` (1): `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:259`.
- ThirdParty `gli` analyzer findings reached from DataPacker (3; listed, never fixed): `gli/core/format.inl:285` `security.ArrayBound`, `gli/core/texture.inl:13` `optin.cplusplus.UninitializedObject`, `gli/texture.hpp:209` `core.uninitialized.Assign`.

Known leftover: `Engine/Source/Frame/Collections/CollectionMemory.h:170-171` compares member addresses through `reinterpret_cast<uintptr_t>`, and its comment says this is because clang rejects `static_cast<const void*>` on `&(T* __restrict)`. With the `__restrict` define in place that comment is likely false.

## Design

User decisions this Plan carries forward:

- Fix everything found; do not stop after tabulation to propose per-area follow-up Plans. The user chose this for the first session; it is this Plan's default.
- The user directed Haiku-model `implementer` workers for diagnostic tabulation and every fix slice, with main reviewing each slice's diff before the review dispatches. Every other role keeps its default model.
- Local generation authorization: a fix under `DataPacker/**`, `Engine/Data/**`, or `Projects/BrokenEngineSandbox/Data/**`, or to `Common/DataFile.h`, is a Local trigger (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`), so every later Client or Server build needs Local data. The executing session requests Local generation authorization for BrokenEngineSandbox builds (`-RunDataPacker`; Gaea export not authorized) at plan approval; the first session's grant does not carry over. Run the generation build as an ordinary build first, then each `-ClangTidy` run without `-RunDataPacker` (`clang-tidy-mode.md` `## Retained protections`).

Steps:

1. **Complete the baseline.** Before any C++ edit, run full `-ClangTidy` Client Release and Server Release builds and add their de-duplicated findings to the `## Context` list. Fix a parse error first, by a meaning-preserving source edit at its site; one that would need a `.clang-tidy` or vcxproj change returns to the user.
2. **Fix** every in-repository site by hand in the form `Documents/C++StyleGuide.txt` prescribes, never with clang-tidy `--fix`:
   - Implicit bool conversion: an explicit comparison (rule 50, `:246`).
   - Unused parameter: `[[maybe_unused]]` (rule 39, `:202`).
   - String by value: `std::string_view`, or `const char*` where rule 40 (`:204`) keeps it (nullable, or reaches a null-terminated API); never `const std::string&`. Other types take `const T&`.
   - C-style cast: a C++ cast (rule 11, `:93`).
   - Every fix preserves meaning, including aliasing of an argument that changes from by-value to by-reference. A dead store is removed only when the value is dead in every configuration.
   - A site in a dual-language shader header (`Engine/Data/Shaders/*.h`, `Projects/BrokenEngineSandbox/Data/Shaders/*.h`) outside a `BT_ENGINE` guard takes a fix that is also valid GLSL, a `NOLINTNEXTLINE`, or a listing, and adds a `/glsl-review`.
3. **Resolve the leftover** at `CollectionMemory.h:170-171`: check whether `static_cast<const void*>` now compiles under both MSVC and the clang-tidy run. If it does, compare through it and drop the comment; otherwise correct the comment to the actual reason.
4. **Record what is not fixed**, for follow-up Plans and the landing summary:
   - A per-site false positive takes `// NOLINTNEXTLINE(<check>) — <reason>`, as at `Engine/Source/Frame/Collections/CollectionController.h:67`.
   - Listed, not fixed: a finding whose only fix changes behavior (a true analyzer defect, use-after-move, or integer division); a finding in a generated `$(GameDataDirectory)` header (the fix belongs in the DataPacker emitter); the ThirdParty `gli` findings; and a check that conflicts with the style guide as a whole, with its rule and count, surfaced to the user.
5. **Final run.** Repeat the five-configuration matrix and record the result from the retained-log warning lines.

Partition fix slices by top-level directory (`Common` with `DataPacker`, `Engine`, `Projects`) so their edits stay disjoint.

## Critical files

- The C++ sources and headers listed in `## Context`, plus any site the Client Release and Server Release runs add
- `Engine/Source/Frame/Collections/CollectionMemory.h` (`IsMemberTupleSubset`)

## In scope

- Meaning-preserving fixes, and `NOLINTNEXTLINE` suppressions with a reason, at the exact sites a `-ClangTidy` run reports in `Common/`, `Engine/`, `DataPacker/`, and `Projects/` source and header files
- The member-address comparison and its comment in `CollectionMemory.h` `IsMemberTupleSubset` (lines 170-171)
- A correction to the `/compile -ClangTidy` mode (`Invoke-CompileBuild.ps1` or `clang-tidy-mode.md`) that a run proves necessary
- `AGENTS.md` updates that a fix makes necessary

## Out of scope

- `ThirdParty/`, and any change to `.clang-tidy` or to a vcxproj's `<ClangTidyChecks>`, `EnableClangTidyCodeAnalysis`, or `RunCodeAnalysis` and any check whose conflict with the style guide is surfaced instead
- `Tools/`: AgentHarness and WorktreeCli enable no clang-tidy, and the header filter omits `Tools`, so adding them would be a configuration change, not a run of the existing one
- Behavior-changing fixes, and DataPacker emitter changes for generated headers; these become follow-up Plans
- Changes to the default `/compile` path, `-Prefast`, or Microsoft code analysis; refactors or style cleanup beyond the reported sites
- Changes to the `-ClangTidy` mode beyond a correction a run proves necessary

## Risk tier and invariants

Expected Tier 3. Trigger: the fixes span independently owned subsystems — `Common`, `Engine`, `DataPacker`, and the game under `Projects/`. Invariants:

- Every C++ fix is meaning-preserving, so PostRender state, CRC, wire, save, replay, and `.pack` formats are unchanged and no `kiVersion` changes.
- A by-value-to-reference or string-view signature change keeps the call semantics, including aliasing of the referenced argument.
- A fix in an unguarded region of a dual-language shader header stays valid GLSL.
- The default `/compile` arguments stay byte-identical, and the `__restrict` define stays visible to clang only.

## Acceptance criteria

- The final full `-ClangTidy` runs of Client Debug, Client Release, Server Debug, Server Release, and DataPacker Release each report no `clang-diagnostic-error`, name the `ClangTidy` target and its clang-tidy invocation in the retained log, and analyze every compile item of the project. Their retained logs contain no warning line for the configured checks, except where a `NOLINTNEXTLINE` with a reason covers it or the landing summary lists it: ThirdParty findings, behavior-changing findings, generated-header findings, and surfaced style-guide conflicts.
- Ordinary `/compile` Client and Server Debug builds succeed, in Local mode after the authorized generation build once a Local trigger changed.
- When any fix touched simulation code (for example `ReconcileReplay.cpp`), an `/agent-harness` replay determinism check passes.
- Diff review finds no `kiVersion` change and no change to a serialized, CRC-participating, wire, save, replay, or `.pack` layout.

## Notes

- `## Sweep` is deliberately absent. Every `/sweep` type finds work by hand-reading each unit file (`.agents/skills/sweep/references/types/style-guide.md:33`, `cpp-adoption.md:8`). Here the work comes from tool diagnostics that only a `/compile -ClangTidy` run produces, and each diagnostic already names its site, so per-file Codex find runs would repeat the tool.
- A check added to `.clang-tidy` before this Plan runs, such as `readability-static-accessed-through-instance` for style rule 72, is part of the configured set at run time and needs no dependency edge.
- Origin: the user's direction in a Claude session on branch `claude/82237093-c04a-4789-8e8b-d5b6fca7a221`; the first execution session (branch `claude/88277bb9-e7bd-4929-a054-141f843a8de5`) built the route and narrowed this Plan to the fixes.
