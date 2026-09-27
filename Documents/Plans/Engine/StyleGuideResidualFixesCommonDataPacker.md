<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:34:43.475Z","dependsOn":[]} -->
# Cleanup: Common and DataPacker — style guide rule 5, 17 and 40 sites whose fix changes a signature, a numeric type or a release path

## Context
The whole-file scanner sweep of `Common/`, `DataPacker/` and `Tools/`
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`)
fixed only meaning-preserving sites (the `/code-style-review` worker step 11
bound) and left these as residuals, because each fix changes a parameter type,
a loop index type or an ownership path. Each is a violation of the named rule
of `Documents/C++StyleGuide.txt` with a fix that keeps behavior:

- Rule 5 (RAII): `DataPacker/Source/ExportJobs/Island/GaeaArchetype.cpp:124`
  `ResolveGaeaExecutable` takes a `_dupenv_s` buffer and frees it by hand with
  `std::free` (`:128`) after constructing a `std::filesystem::path` from it
  (`:127`); that
  constructor can throw, which leaks the buffer.
- Rule 17 (`size_t` vector indices): `int64_t` loop indices compared against
  `static_cast<int64_t>(v.size())` at
  `DataPacker/Source/ExportJobs/ExportScene.cpp:619,637,679,719,1014` and
  `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:82,189,214`.
- Rule 40 (`std::string_view` parameters):
  - `Common/Log/DiagnosticLog.h:12` / `Common/Log/DiagnosticLog.cpp:6`
    `const char* pcFilename`, used only to build a `std::filesystem::path` and
    open the `std::ofstream`, both of which accept a path.
  - `Common/Serialization.h:31` `ValidateDeserializedCount` and `:42`
    `ValidateDeserializedCountCapacity` `const char* pcReader`, used only to
    construct `std::ios_base::failure` on the throw path; every caller passes a
    string literal (for example `Engine/Source/File/DifferenceStream.h:313`).
  - `DataPacker/Source/FileManager.cpp:53` `RunGit` `const wchar_t* pcArguments`,
    only appended to a `std::wstring`; callers pass literals (`:201`, `:202`).
  - `DataPacker/Source/ExportJobs/ExportShaderDependencies.cpp:187`
    `ParseWhitespaceDependencies` `const std::string& rContent`, read through a
    `std::istringstream` and formatted into an error message.

## Design
The author's recommendation:
1. Rule 5: hold the `_dupenv_s` buffer in a `std::unique_ptr<char, ...>` whose
   deleter calls `std::free`, created immediately after the call, and drop the
   manual `std::free`. The path is still built from the buffer before it goes
   out of scope, so the lookup order and result are unchanged.
2. Rule 17: make each listed loop index `size_t` and drop the
   `static_cast<int64_t>` on the bound. Where the body mixes the index with a
   signed value (`ExportScene.cpp:684` compares `iOrigMat != i`), cast the
   index at that use so the comparison keeps its current signed meaning; the
   existing `static_cast<int>(i)` uses (`ExportScene.cpp:682`,
   `SceneSkeletonLoader.cpp:221,226`) and `LOG` arguments need no change.
3. Rule 40: change each listed parameter to `std::string_view` /
   `std::wstring_view`. Build the owned value only where a consumer needs one:
   `std::filesystem::path(view)` for `DiagnosticLog`,
   `std::ios_base::failure(std::string(view))` on the Serialization throw path,
   `std::wstring(view)` in `RunGit`'s concatenation, and `std::ispanstream` (C++23)
   over the view in `ParseWhitespaceDependencies`. Callers convert implicitly.

Rationale: every fix uses a standard mechanism already available, keeps each
call site unchanged, and leaves the produced files and messages byte-identical.

## Critical files
- `DataPacker/Source/ExportJobs/Island/GaeaArchetype.cpp`
- `DataPacker/Source/ExportJobs/ExportScene.cpp`
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp`
- `Common/Log/DiagnosticLog.h`, `Common/Log/DiagnosticLog.cpp`
- `Common/Serialization.h`
- `DataPacker/Source/FileManager.cpp`
- `DataPacker/Source/ExportJobs/ExportShaderDependencies.cpp`

## In scope
- `ResolveGaeaExecutable` in `GaeaArchetype.cpp` (rule 5)
- The eight loop indices listed in `## Context` and the index uses inside those
  loop bodies (rule 17)
- The parameters of `DiagnosticLog::DiagnosticLog`,
  `ValidateDeserializedCount`, `ValidateDeserializedCountCapacity`, `RunGit` and
  `ParseWhitespaceDependencies`, and the in-body uses of those parameters
  (rule 40)

## Out of scope
- Any caller edit beyond what the implicit conversions already cover
- These scanner-sweep residuals, not planned because the rule's form cannot be
  reached without changing an external or deliberate constraint:
  - Rule 40, value reaches a consumer that needs null termination or stores
    the pointer: `Common/FileUtils.h:82` (`CreateFileW` on the crash path),
    `Common/Log/LogDifference.h:11` (stored in `gpLogDifferenceContext`),
    `DataPacker/Source/ExportJobs/ExportJob.cpp:49` (`memcmp` over `size()+1`
    bytes), `DataPacker/Source/FileManager.cpp:28` (`SearchPathW`),
    `Tools/ToolCommon/CoordinationStore.cpp:106`,
    `Tools/ToolCommon/CoordinationStore.h:43`,
    `Tools/WorktreeCli/PlanMetadata.cpp:16` and `Tools/WorktreeCli/PlanMetadata.h:26`
    (`std::sscanf` through `c_str()`), `Tools/WorktreeCli/BuildCommand.cpp:39`
    (`GetEnvironmentVariableW`), `Tools/WorktreeCli/PlanScheduler.cpp:54`
    (`nlohmann::json` C-string key)
  - Rule 40, parameter type fixed elsewhere:
    `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:10,381` (lookup
    in a ThirdParty tinygltf `std::map<std::string, ...>` without a transparent
    comparator), `DataPacker/Source/Main.cpp:46` (`common::ContentsEqual`
    accepts only `std::string` or a path), and the byte-buffer pointer plus
    length at `Tools/AgentHarness/AgentHarness.cpp:247` and
    `Tools/WorktreeCli/BuildCommand.cpp:135,188` (not a string parameter)
  - Rule 17, value whose width a consumer fixes or whose signed arithmetic
    depends on it: `Common/Threading/Multithreading.h:39`,
    `DataPacker/Source/ExportJobs/AudioRepair.cpp:57,319,373,389,492`,
    `ExportAudio.cpp:85`, `ExportIsland.cpp:120,671,672,673,674`,
    `ExportScene.cpp:790,1023,1034`, `ExportShader.cpp:254`,
    `ExportShaderDependencies.cpp:295`, `ExportTexture.cpp:115,226,264,323`,
    `Island/ProcessBakedRegion.cpp:271,272`,
    `Island/SubdivideBeachBand.cpp:130,161,204`,
    `Scene/SceneVerticesLoader.cpp:92,127,369`, `Texture/Texture.cpp:736`
    (paths under `DataPacker/Source/ExportJobs/`)
  - Rule 6: `Common/ExternalHeaders.h:18,392,394` configure and check
    third-party debug macros in the preprocessor, where no `kb*` toggle can be
    tested
  - Rule 15: `Common/Log/LogDifference.h:23` returns a different type per
    `if constexpr` branch; naming it needs a new type trait
  - Rule 28: `DataPacker/Source/Main.cpp:960,961,964,965` `NULL` inside the SAL
    `_Success_(return != NULL)` annotation that mirrors the CRT's own
    replaceable `operator new` declarations
- Rule 18, 32, 35 and 44 residuals (their own follow-up Plans); `ThirdParty/`

## Risk tier and invariants
Tier 2 (scoped behavior): trigger is a public signature change in shared
`Common/` headers (`Serialization.h`, `DiagnosticLog.h`) with no change to
what any caller computes. Serialized and `.pack` bytes, sim state and CRCs
stay identical; no format or `kiVersion` changes.

## Acceptance criteria
- `/compile` passes for DataPacker (Release) and Client and Server in Debug and
  Release
- A DataPacker export of the unchanged assets produces byte-identical `.pack`
  output
- An `/agent-harness` replay determinism check passes

## Notes
Originating record: the scanner sweep's adjudication of these sites as
residuals outside the step 11 meaning-preserving bound.
