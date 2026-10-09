<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:17:38.738Z","dependsOn":[]} -->
# Remove over-engineered checks, fallbacks, and dead generality from Common

## Context

A repository-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, ultra-rare edge-case protection, and speculative generality that should be a plain operation plus an `ASSERT` or a hard error. A low-effort finder model flagged candidates per file, and one validation pass by a stronger model confirmed or rejected each against the code. The main session spot-checked only two validation groups, and when this Plan was written, a check found every candidate below still present in the source. The sweep's working files were temporary and are gone. This Plan carries the `Common/**` results inline: 7 of the 8 confirmed candidates and 1 rejection. The eighth, candidate 1 (the `~DiagnosticLog` teardown guard), moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`; the remaining candidates keep their original numbers.

**Recorded user direction (relayed by the dispatching session):** the candidates below must not be trusted blindly. The executing agent re-verifies each one independently against current code; line numbers have drifted, so locate each by symbol. It applies only a candidate it can prove itself, citing the invariant or every caller, and it drops and reports any candidate that does not hold. Each candidate is a proposal, not an approved change.

## Design

### Re-verification protocol (per candidate)

1. Locate the code by symbol. Confirm that the quoted check, parameter, or function still exists.
2. Re-derive the validator's evidence yourself: every caller (`git grep` outside `ThirdParty/`), every writer of the guarded state, and the invariant that makes the case impossible. If any caller, writer, or path makes the case reachable, drop the candidate and report it with that `path:line`.
3. Apply the simpler form. The `cpp-conventions.md` "No useless ASSERTs" rule decides whether to replace a check with an `ASSERT` or delete it outright. If the very next statement would dereference the same pointer, or the same index would be bounds-checked by `.at()`, delete the check without an `ASSERT`. If the impossible case would otherwise be undefined behavior without a guaranteed fault, use `ASSERT(<cond>)`.
4. If a candidate's proof needs code outside `## In scope`, drop it and report it. Do not widen scope.

### Boundaries that are never removed

- Server validation of every client-to-server record and every grid save. A bad value read from a packet or file is rejected and never clamped or substituted (`.agents/references/cpp-conventions.md` bad-value rule).
- The per-tick determinism CRC and anything that feeds it.
- `.pack`/manifest/save/replay format and version checks.
- Win32 and Vulkan API result checks on calls that remain, and checks on external input: user files, launch arguments, third-party tool output, and OS state the program does not control.

Converting a check into an `ASSERT` is an acceptable outcome.

### Candidates (validator evidence in brief)

1. **Moved.** The `~DiagnosticLog` teardown guard moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`.
2. **`Common/Log/Log.h` `LogBuffer::Tail` (~53-76).** The non-wrapping `else` branch is never instantiated. The only `Tail` caller is `CollectLogLines` (`Engine/Source/Agent/AgentCommandsShared.cpp` ~69). It is instantiated only with `gLogRingBuffers[...]` and `gLogAgentBuffer` (~140, ~145), and both are `WRAP=true` (`Log.h` ~114, ~116). `gLogGlobalBuffer` (`WRAP=false`) uses only `AcquireLine` and `Dump` (`Log.cpp` ~195, ~202). Simpler form: delete the `if constexpr (WRAP)`/`else` split, keep only the wrap loop, and add `static_assert(WRAP);` at the top of `Tail`. The wrap math would return wrong lines on a frozen buffer.
3. **`Common/Math/MathUtils.h` `consteval int64_t Ceiling(float)` (~14-17).** No callers outside `ThirdParty/`; every other `Ceiling(` match is PowerShell `[Math]::Ceiling`. Simpler form: delete the function and the blank line that follows it.
4. **`Common/Threading/ThreadLocal.h` (~76-78) and `ThreadLocal.cpp` (~6-8).** The `iWorkbufferReserveSize` parameter of the private constructor is always 0. The only call site is `ThreadLocalEntry::operator()` (`ThreadLocal.h` ~101), which passes three arguments, and `Entry` (~48) offers no way to pass a fourth. Simpler form: drop the parameter from the declaration and the definition, and initialize `mWorkbufferMemory(64 * std::max(iWorkbufferSize, 64i64 * 1'024i64))`. Reword the comment to keep only the floor rationale: the reserve is 64x the initial size, floored at 64 KiB first, so a thread with a small initial workbuffer can still grow. The reserve value is unchanged.
5. **`Common/WindowsUtils.cpp` `HardwareCoreCount` (~48-61).** The `GetModuleHandle(TEXT("kernel32")) == nullptr` and `GetProcAddress(..., "GetLogicalProcessorInformation") == nullptr` fallbacks are unreachable, and so is the dynamic lookup they serve. kernel32 is mapped into every Win32 process. The build targets `_WIN32_WINNT_WIN10` (`Common/ExternalHeaders.h` ~40), where `GetLogicalProcessorInformation` is declared and linked through kernel32.lib. Simpler form, one change: delete the `LPFN_GLPI` alias, `GetModuleHandle`, `GetProcAddress`, and both fallbacks, and call `GetLogicalProcessorInformation(buffer.data(), &uiReturnLength)` directly. This removes the lookup calls themselves. The result check on the remaining `GetLogicalProcessorInformation` call (~71-83) stays.
6. **`Common/Workbuffer.h` `ScopedWorkbufferAllocation::Adopt` (~239-246).** No callers outside `ThirdParty/`. Other "Adopt" matches are a comment at ~253 and unrelated prose. Simpler form: delete the comment and the function, and change the constructor comment at ~253 to `// Frame already opened by RawPushBuffer; capture the top depth.`
7. **`Common/Workbuffer.h` `std::formatter<common::ScopedWorkbufferAllocation<const char*>>` (~344-352).** The specialization is unreachable. Allocations are constructed only by `Workbuffer::PushBuffer<T>` (~277-279) and `Adopt`. No `PushBuffer<const char*>` exists (the char users are `PushBuffer<char*>` at `Engine/Source/Ui/MenuUtils.cpp` ~16 and `Engine/Source/Graphics/GraphicsUtils.cpp` ~39), and `Adopt` has no callers. Simpler form: delete the specialization and its preceding blank line. Re-verify this candidate together with candidate 6.

The validation pass counted candidate 5 as two findings (the module-handle fallback and the proc-address fallback), which is why the validated count is 8, of which 7 remain here.

## Critical files

- `Common/Log/Log.h`
- `Common/Math/MathUtils.h`
- `Common/Threading/ThreadLocal.h`
- `Common/Threading/ThreadLocal.cpp`
- `Common/WindowsUtils.cpp`
- `Common/Workbuffer.h`

## In scope

- `LogBuffer::Tail` in `Log.h`.
- `Ceiling(float)` in `MathUtils.h`.
- The private `ThreadLocal` constructor declaration, its definition, its `mWorkbufferMemory` initializer, and the comment above the declaration.
- `HardwareCoreCount` in `WindowsUtils.cpp`, the dynamic-lookup region and the call site of `GetLogicalProcessorInformation` only.
- `ScopedWorkbufferAllocation::Adopt`, the comment on the `ScopedWorkbufferAllocation` private constructor, and the `std::formatter<common::ScopedWorkbufferAllocation<const char*>>` specialization in `Workbuffer.h`.

## Out of scope

- **Do not change (rejected by validation):** `Common/ErrorUtils.cpp` `CheckHresult` (~26) re-tests `iHresult < 0` even though `CHECK_HRESULT` already tested it. It mirrors `common::Assert` (`ErrorUtils.cpp` ~16), which must keep its own test because `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.cpp` calls it directly. Removing the test would leave a public `CheckHresult` that throws even for a success value.
- The `~DiagnosticLog` and `~Multithreading` teardown guards: `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles the singleton teardown guard family once.
- Every other `Common/` file and function, including the `GetLogicalProcessorInformation` result check and buffer-resize loop in `HardwareCoreCount`, the `WRAP=false` behavior of `AcquireLine` and `Dump`, and every other `std::formatter` specialization in `Workbuffer.h`.
- Any candidate the executor cannot prove. Report it; do not substitute a different change for it.
- Every boundary listed under `### Boundaries that are never removed`.

## Acceptance criteria

- Every candidate is either applied with the executor's own cited proof (the invariant or every caller) or dropped and reported with the `path:line` that makes it reachable.
- Client, server, and DataPacker compile in Debug and Release with no new warnings.
- An `/agent-harness` log query that goes through `CollectLogLines` returns the same recent lines as before, and the client and server start and shut down cleanly.

## Notes

- Change Workflow tier: **Tier 2**. The trigger is the removal of public `Common` header functions (`Ceiling`, `Adopt`, a `std::formatter` specialization) and a changed Win32 call path in `HardwareCoreCount`, all inside one subsystem. There is no determinism/CRC, wire, serialization, threading, or trust-boundary exposure: the `ThreadLocal` reserve size is unchanged, and no simulation code is touched.
- Exposure: `Common` compiles into the client, the server, and DataPacker, so a compile through `/compile` covers every consumer of the removed symbols.
- No `AGENTS.md` or documentation names any symbol this Plan touches.
- Sibling sweep Plans: `Documents/Plans/Engine/RemoveAgentOverEngineering.md` and `Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md`. They are independent: no ordering, and no shared regions.
