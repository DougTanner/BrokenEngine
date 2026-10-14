<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T15:47:59.485Z","dependsOn":[]} -->
# Remove the four per-site MSVC warning-suppression pragmas from repository source

## Context

The style rule against suppression comments and pragmas in code (rule 75 in `Documents/C++StyleGuide.txt`, plus the suppression paragraph of `.agents/skills/repo-code-review/references/checks.md`) says a diagnostic is either fixed in code or surfaced to the user, who may turn the warning off in project configuration instead of in the source file. The user's rule: "in the future we shouldn't blindly add no-lint comments, the code should either be fixed or the problem surfaced to the user", and "we turn them off at the source not pollute the code files". The user ruled that the third-party include wrappers (`Common/ExternalHeaders.h` `push(0)`/`pop` blocks, `Tools/ToolCommon/ToolCliCommon.h:17-20`) and the project-wide disable list in `Common/ExternalHeaders.h:415-444` count as configuration and stay. Four per-site suppressions remain in first-party source:

| Site | Suppression | Warning | Why it fires |
|---|---|---|---|
| `Common/Log/Log.cpp:7-10` | `push` / `disable : 4073` / `pop` around `#pragma init_seg(lib)` | C4073, level 1, "initializers put in library initialization area" | The compiler emits it for every `init_seg(lib)`. The placement is load-bearing: `Log.cpp` holds non-trivially constructed and destroyed globals (`gLogMutex`, `gLogRingBuffers`, `sLogFileStream`, `sLogAliveGuard`, `Log.cpp:17-60`) that must outlive every default-phase static (`Common/Log/AGENTS.md`, `Engine/Source/Memory/GlobalAllocator.cpp:288`). |
| `Engine/Source/File/FileManager.cpp:93` | `suppress: 26492` | C26492, Core Guidelines type.3, `const_cast` casts away const | `Sha256Hasher::Update` passes `std::span<const std::byte>` data to `BCryptHashData`, whose `pbInput` is a non-const `PUCHAR`. The same cast without a pragma exists at `DataPacker/Source/InputFingerprint.cpp:44` and `Tools/ToolCommon/CoordinationStore.cpp:162`; neither project runs the rule set that reports C26xxx codes. |
| `Engine/Source/File/PackChunks.cpp:986` | `suppress: 6250` | C6250, `VirtualFree` without `MEM_RELEASE` leaves the address reservation | `PackChunks::DecommitChunkRange` decommits the page-aligned interior on purpose; `RecommitAndReloadChunkRange` recommits the same addresses inside the kept lazy-pool reservation. |
| `Engine/Source/Graphics/Managers/TextureCache.cpp:94` | `suppress: 6387` | C6387, an argument may be null for `memcpy` | `stagingBuffer.vmaAllocationInfo.pMappedData` carries no analyzer-visible non-null fact before the `std::memcpy`. |

Deleting a pragma without a fix fails the build. Client and server treat warnings as errors in every configuration, so C4073 fails every client and server build, and DataPacker Release, which also compiles `Log.cpp`. C6250 and C6387 come from `/analyze` default rules, which Release runs in every client and server build (`EnablePREfast`, under `TreatWarningAsError` or `CodeAnalysisTreatWarningsAsErrors`). C26492 is reported only where `BrokenEngineAnalysis.ruleset` applies: Visual Studio Release builds and `/compile` PREfast mode. `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` `## Build Configuration` documents both analysis paths.

## Design

For each site, remove the pragma and apply one resolution. Rule 75 leaves fix versus disable to the user for each warning: the session presents the warning and why it fires, and applies a disable only after the user approves it. The recommendations below are the author's.

1. `TextureCache.cpp:94` — code fix if one exists, otherwise surfaced to the user. Do not add an `ASSERT` whose only purpose is to silence C6387: `.agents/skills/repo-code-review/references/checks.md` `### ASSERT behavior` rejects it, because a null pointer would already fault at the `memcpy`. The PREfast build first shows which `memcpy` argument C6387 names. Apply a code fix only if the analyzer can see its guard on the reachable path and it preserves meaning. If no such fix exists, present the diagnostic to the user, who either chooses a different fix or approves a disable of 6387 in the `Common/ExternalHeaders.h` disable list.
2. `PackChunks.cpp:986` — a disable of 6250 in the `Common/ExternalHeaders.h` disable list, surfaced to the user (recommended). The decommit is the intended design, and no other API keeps the same semantics: `MEM_RESET`, `DiscardVirtualMemory`, and `OfferVirtualMemory` keep the commit charge and turn the accidental-read fault into a silent stale read. Calling `VirtualFreeEx` only to dodge the check is a workaround, not a fix. Keep the existing explanatory comment above the call.
3. `FileManager.cpp:93` — a disable of 26492 in the `Common/ExternalHeaders.h` disable list, surfaced to the user (recommended). The CNG signature forces the cast. Copying into a mutable buffer adds a full copy to replay-digest and file hashing. Making `Update` take a mutable span only moves the cast to callers that hold const data (`FileManager::ComputeSha256`).
4. `Log.cpp:7-10` — a disable of 4073 in the `Common/ExternalHeaders.h` disable list, surfaced to the user (recommended). Keep `#pragma init_seg(lib)` itself; it is the mechanism, not a suppression. Moving the TU's globals to construct-on-first-use or never-destroyed storage changes logging lifetime across static destruction, which is out of scope. Naming an explicit CRT section instead of `lib` is unverified to avoid a warning (C4075 may fire instead); it would need `/verify-external-claims`, and the author does not recommend it.

An approved disable goes in the project-wide disable list `Common/ExternalHeaders.h:415-444`, one line each in that list's comment format (author's recommendation: it covers client, server, and DataPacker from one place). Do not add per-code exceptions to `BrokenEngineAnalysis.ruleset`, which is `IncludeAll` by design, and do not drop `EnablePREfast`.

If the user declines a disable and no code fix is found, that site stays a blocker returned to the user. Never keep a pragma.

## Critical files

- `Common/Log/Log.cpp`
- `Engine/Source/File/FileManager.cpp`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/Graphics/Managers/TextureCache.cpp`
- `Common/ExternalHeaders.h` (only for a user-approved disable)

## In scope

- `Log.cpp:7-10`: delete the `push`, `disable : 4073`, and `pop` lines; keep `#pragma init_seg(lib)` and the comment above it.
- `FileManager.cpp:93` in `Sha256Hasher::Update`: delete the `suppress: 26492` line.
- `PackChunks.cpp:986` in `PackChunks::DecommitChunkRange`: delete the `suppress: 6250` line.
- `TextureCache.cpp:93-94` in the readback function ending at `:95`: delete the `suppress: 6387` line, plus the comment above it if it no longer explains the code, and apply the Design step 1 resolution.
- A user-approved disable for 4073, 6250, 6387, or 26492 in the `Common/ExternalHeaders.h` disable list.

## Out of scope

- The third-party include wrappers and the existing `Common/ExternalHeaders.h` disable list entries (user decision: configuration, stays).
- The vcxproj `4702` disable and the unsuppressed `const_cast` sites in `InputFingerprint.cpp` and `CoordinationStore.cpp`.
- Any change to `init_seg` placement, logging lifetime, the lazy-pool decommit and recommit semantics, or the hashing API.

## Risk tier and invariants

Expected Tier 1 (trigger: local behavior-preserving work with no public signature or invariant exposure; it deletes suppression lines, applies at most one meaning-preserving local code fix, and adds warning-configuration lines that change no generated code). A resolution that changes initialization placement, decommit semantics, or the hashing call path is a Tier 2 escalation; return the Plan for re-planning first.

- No simulation, CRC, serialization, `.pack`, replay, wire, or threading surface changes.

## Acceptance criteria

- `git grep -n -E "pragma warning\((push|pop|disable|suppress)" -- Common Engine DataPacker Projects Tools` lists only `Common/ExternalHeaders.h` and `Tools/ToolCommon/ToolCliCommon.h`.
- Through `/compile`: client and server build Debug and Release; DataPacker builds Release; client and server Release build in PREfast mode (`.agents/skills/compile/references/prefast-mode.md`), which this Plan explicitly requires. Every build has zero warnings.
- Every applied disable has the user's recorded approval.

## Coordination

- Resolve the C26492 site in the engine `Sha256Hasher::Update` (`FileManager.cpp:93`) where it stands; `Documents/Plans/Engine/SharedSha256Helper.md` moves that `Update` into `common::Sha256Hasher` in `Common/WindowsUtils.cpp` in the form this Plan leaves.

## Notes

- Origin: `/next-plan` run for `Documents/Plans/Engine/FixClangTidyDiagnostics.md`, user decision U1 at its landing confirmation; client claude, worktree/branch UUID 26f51454-1483-4a0c-9553-d881e5d3bb24, branch `claude/26f51454-1483-4a0c-9553-d881e5d3bb24`, conversation session ID a66d08fc-bcd2-47ab-a78e-fa6e832954aa.
- The four sites were verified at HEAD 166f3dd5.
