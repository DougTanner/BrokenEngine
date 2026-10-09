<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:17:45.723Z","dependsOn":[]} -->
# Remove over-engineered checks, fallbacks, and dead generality from the Audio, Frame, Input, Profile, and Ui engine runtime

## Context

A repository-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, ultra-rare edge-case protection, and speculative generality that should be a plain operation plus an `ASSERT` or a hard error. A low-effort finder model flagged candidates per file, and one validation pass by a stronger model confirmed or rejected each against the code. The main session spot-checked only two validation groups, and when this Plan was written, a check found every candidate below still present in the source. The sweep's working files were temporary and are gone.

This Plan carries the inline results for these areas:

- `Engine/Source/Audio`, `Frame`, `Input`, `Profile`, and `Ui`;
- `Engine/Source/CrashReport.cpp` and `Engine/Source/LaunchOptions.cpp`.

There are 24 confirmed candidates and 1 rejection; 22 remain here. Candidates 14 and 16 (the `~IslandTerrainResidency` and `~RawInputManager` teardown guards) moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`, and the remaining candidates keep their original numbers. `Engine/Source/Network`, `Server`, `File`, and `Graphics` are handled separately.

**Recorded user direction (relayed by the dispatching session):** the candidates below must not be trusted blindly. The executing agent re-verifies each one independently against current code; line numbers have drifted, so locate each by symbol. It applies only a candidate it can prove itself, citing the invariant or every caller, and it drops and reports any candidate that does not hold. Each candidate is a proposal, not an approved change.

## Design

### Re-verification protocol (per candidate)

1. Locate the code by symbol. Confirm that the quoted check, parameter, or function still exists.
2. Re-derive the validator's evidence yourself: every caller (`git grep` outside `ThirdParty/`, including `Projects/`), every writer of the guarded state, and the invariant that makes the case impossible. If any caller, writer, or path makes the case reachable, drop the candidate and report it with that `path:line`.
3. For a candidate in deterministic simulation code (candidates 8-13 below), also prove that the change is bit-identical for every value the simulation consumes. If that proof fails, drop the candidate. Never accept a CRC change.
4. Apply the simpler form. The `cpp-conventions.md` "No useless ASSERTs" rule decides whether to replace a check with an `ASSERT` or delete it outright. If the very next statement would dereference the same pointer, or the same index would be bounds-checked by `.at()`, delete the check without an `ASSERT`. If the impossible case would otherwise be undefined behavior without a guaranteed fault, use `ASSERT(<cond>)`. `ASSERT` is active in Release, so the author recommends leaving it out of per-pair or per-element simulation loops when phase ordering already establishes the invariant.
5. If a candidate's proof needs code outside `## In scope`, drop it and report it. Do not widen scope.

### Boundaries that are never removed

- Server validation of every client-to-server record and every grid save. A bad value read from a packet or file is rejected and never clamped or substituted (`.agents/references/cpp-conventions.md` bad-value rule).
- The per-tick determinism CRC and anything that feeds it. PostRender state must stay bit-identical.
- `.pack`/manifest/save/replay format and version checks.
- Win32 and Vulkan API result checks on calls that remain, and checks on external input: launch arguments, user files, third-party library output, and OS state the program does not control.

Converting a check into an `ASSERT` is an acceptable outcome.

### Candidates (validator evidence in brief)

**Audio**

1. **`StaticVoices::PlayOneShot` / `AudioManager::PlayOneShot` `bool` parameter.** The parameter is `bThreeDimensional` in `StaticVoices.h` ~44 and `StaticVoices.cpp` ~21-39, and `b3d` in `AudioManager.h` ~51 and `AudioManager.cpp` ~303-310. It has one value in use: the only caller, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp` ~195, passes `false`. Passing `true` would load a 3D voice but skip both the `SetVolume` call and the 3D mix, so it is not a usable mode. Simpler form: drop the parameter from both functions. Call `PlayOneShotLocked(uiAudioCrc, false, fVolume, fPitch, fPitchRange)` and `mpStaticVoices->PlayOneShot(rFrame, uiAudioCrc, fVolume, fPitch, fPitchRange)`, and remove the `false,` argument at the `PlayersCombat.cpp` caller. `PlayOneShotLocked` keeps its parameter for the 3D path.
2. **`StreamingVoices::Clear`, null check on `mPreviousStreams` elements (~151-154).** The only insertion is `push_back(std::move(mpCurrentStream))` (~208), right after a dereference of `mpCurrentStream` (~207). The harness rig only reads the vector, and the sibling loops (~101-104, ~128-131, ~203-206) already dereference the elements without a check. Simpler form: `rpStream->mpVoice = nullptr;` with no `if`.
3. **`StreamingVoices::Clear`, null check on `mStreamsToDestroy` elements (~159-163).** This check is moot, because candidate 4 deletes the whole loop.
4. **`StreamingVoices::Clear`, both loops over `mStreamsToDestroy` (~137-140 and ~157-163).** The loops never run, because the vector is always empty when `Clear` runs. Its only writer is `StreamingVoices::Update` (~111), and `Update` clears it unconditionally at its end (~119). Nothing between those two lines reaches `Clear`. `Update` (`AudioManager.cpp` ~494) and every `Clear` caller (`AudioManager.cpp` ~261; `AudioStreamingHarnessRig.cpp` ~128, ~177, ~219) are separate main-thread calls. Simpler form: delete both loops. The author recommends keeping the `mStreamsToDestroy` member and its `Update` use (see `## Out of scope`) to keep this change minimal.

**Crash report and launch options**

5. **`Engine/Source/CrashReport.cpp` (~37-40).** The error branch on `wcscpy_s` is unreachable. The only caller (`Engine/Source/Main.cpp` ~833-835) passes the non-null `c_str()` of a non-empty string, and the earlier length returns (~27, ~32) guarantee that the source and its terminator fit. Simpler form: `wcscpy_s(spcAppDataOverride, std::size(spcAppDataOverride), pcDirectory);` without a result branch.
6. **`Engine/Source/LaunchOptions.cpp` `--windowed` parsing (~156).** The `pcEnd != nullptr &&` term is always true: given a non-null `str_end`, `wcstoll` always stores a pointer into the input. Simpler form: delete only that term.
7. **`LaunchOptions.cpp` `--windowed` validation (~160).** The `pcEnd == nullptr ||` term is always false. Both `wcstoll` calls pass a non-null `&pcEnd`. Simpler form: delete only that term. The user-input checks stay: `pcEnd != pcValue`, the `x`/`X` separator, the range bounds, and the `*pcEnd != L'\0'` trailing-garbage rejection.

**Frame (deterministic simulation; see protocol step 3)**

8. **`Collision.cpp` `TestAndCollectPair` (~500-505).** The `kAlreadyCollided` test repeats filtering that has already happened. The engine sets the flag only in `CommitCandidate` (~477, ~481), which runs after every `CollideLayerPair` call in `Collide` (~347-362). The game sets it only when it fills the collision-flag inputs before collision (`MissilesUpdate.cpp` ~321, `Players.cpp` ~633, `SpaceshipsCombat.cpp` ~100). A-side objects with the flag are skipped (~612), and B-side objects with the flag are never inserted into the zones (`InsertLayerObjectsIntoZones` ~187). The sole caller is at ~647. Simpler form: delete the comment and the `if` block. The candidate set is unchanged, so this is a determinism no-op. The author recommends no `ASSERT` in this per-pair hot path.
9. **`IslandChainPlacement.cpp` `OrientForTangent` (~156).** The `fShort <= 0.0f ||` term can never be true. `fQuadFootprintX` and `fQuadFootprintY` are written only at `IslandTerrain.cpp` ~41-42, from header values that ~39-40 assert are `> 0.0f`. Simpler form: `if (fLong / fShort < kfLongAspectThreshold)`.
10. **`IslandTerrain.cpp` server NavContour loop (~216-224, `#if defined(BT_SERVER)`).** The `rTemplate.puiHeightmapHalf != nullptr` wrapper can never fail. The preceding loop over the same `mIslands` (~137-208) either throws on a null `pData` (~146-150) or assigns a non-null `puiHeightmapHalf` (~182), its only writer. `mIslands` is populated only in the constructor (~34). Simpler form: remove the wrapper and its braces. The author recommends no `ASSERT`, because the conversion call dereferences the pointer immediately.
11. **`NavBuild.cpp` CCW-assert loop (~428-431).** `if (iCount < 3) { continue; }` never fires. The only producer of the contour's polygon offsets and vertices is the push loop (~392-402), which runs only for paths that passed `std::ssize(rPath) < 3 -> continue` (~383) and pushes every point. The sole caller (`IslandTerrain.cpp` ~223) passes the template's own `navContour`. Simpler form: delete the `if`.
12. **`NavBuild.cpp` marching-squares `Lerp` (~83).** `std::clamp(..., 0.0f, 1.0f)` is a no-op on every value used. Each case consumes only crossings on edges whose two corners straddle the threshold (classification ~48-63; saddle cases 5 and 10 straddle on all four edges). For a straddling pair, `|fA - T| <= |fA - fB|` with the same sign, and IEEE subtraction and division are monotone, so the quotient is already in [0, 1]. `std::clamp` also returns a `-0.0f` input unchanged. Simpler form: `return (fA - fThresholdValue) / fDenominator;`. The executor proves the result bit-identical for every crossing a case consumes.
13. **`NavQuery.cpp` `AStarPath` walk-back loop (~451-452).** The `rMemory.pParent[iNode] != -1` clause is unreachable:
    - A node is pushed only from `TryNeighbor` after its parent is set (~482-490).
    - A parent is always the closed, currently expanded node, whose parent is never rewritten (~468).
    - Only the start node keeps parent `-1` (~412).
    - `iStartNode` (`iVertexCount`) never equals `iEndNode` (`iVertexCount + 1`).

    Simpler form: `while (rMemory.pParent[iNode] != iStartNode)`, with `ASSERT(rMemory.pParent[iNode] != -1);` inside the loop, because indexing `pParent[-1]` would be undefined behavior without a guaranteed fault. This runs once per found path, not per node expansion.
14. **Moved.** The `~IslandTerrainResidency` teardown guard moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`.

**Input**

15. **`RawInputManager.cpp` constructor `catch (...)` (~24-27) after `catch (const std::exception&)` around `std::make_unique<GamePad>()`.** The client builds with `/EHa` (`BrokenEngineSandbox.vcxproj` `<ExceptionHandling>Async</ExceptionHandling>`), so `catch (...)` also catches SEH faults. Every C++ throw in DirectXTK `GamePad.cpp`, plus `std::bad_alloc`, derives from `std::exception` (`com_exception : public std::exception` in `PlatformHelpers.h`). The only thing left for this handler to catch is an access violation or another SEH fault, which it would hide instead of sending to the crash-report path. Simpler form: delete the `catch (...)` handler. Re-verify the throw list against the explicit `ThirdParty/DirectXTK` file paths.
16. **Moved.** The `~RawInputManager` teardown guard moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`.
17. **`RawInputManager::SetVibration` defaulted `fLeftTrigger` and `fRightTrigger` (`RawInputManager.h` ~55, `.cpp` ~89-93).** No caller supplies them. The only caller is `Engine/Source/Graphics/CameraBase.cpp` ~195, `SetVibration(0, fVibration, fVibration)`. Simpler form: remove both parameters, and pass `0.0f, 0.0f` to `mpGamePad->SetVibration`.
18. **`RawInputManager::SetVibration` `int64_t iPlayer`.** The parameter only ever receives 0. `Update` already hardcodes player 0 in `GetState(0)` and `GetCapabilities(0)`, with the comment "only first game pad supported" (~175-184). Simpler form: remove the parameter, call `mpGamePad->SetVibration(0, fLeftMotor, fRightMotor, 0.0f, 0.0f)`, and update `CameraBase.cpp` ~195 to `SetVibration(fVibration, fVibration)`. Apply it together with candidate 17.

**Profile**

19. **`ProfileManagerBase::LatchRawCpuTimer(int64_t, bool)` (`.cpp` ~271-293, `.h` ~354).** No callers. Every latch site calls `LatchRawCpuTimers` (`GameBase.cpp` ~384, ~410, ~427; `Replay.cpp` ~393; `GameSaveLoad.cpp` ~60, ~86). Simpler form: delete the definition and the declaration.
20. **`ProfileManagerBase::ArmRawCpuTimerEvent` (`.cpp` ~337-348, `.h` ~356).** This locking wrapper has no callers. The only arm site calls `ArmRawCpuTimerEventLocked` (`ServerSimulationHarnessRigs.cpp` ~790). Simpler form: delete the wrapper's definition and declaration, and leave `ArmRawCpuTimerEventLocked` unchanged.
21. **`ProfileManagerBase::AcknowledgeRawCpuTimerEvent` (~424).** The `iEventSequence == 0 ||` term is redundant. `kAvailable` is set only in `PublishRawCpuTimerEvent`, after `++rEvent.iEventSequence` (~401-407), which starts at 0. So while `kAvailable` is set, the stored sequence is at least 1, and `iEventSequence != rEvent.iEventSequence` already rejects 0. The sole caller, the trusted agent channel (`AgentCommandsServer.cpp` ~218), also rejects 0 (~195). Simpler form: `if (!(rEvent.flags & RawCpuTimerEventFlags::kAvailable) || iEventSequence != rEvent.iEventSequence)`.
22. **`ProfileManagerBase` cross-thread `CpuStop` search (~191).** The `std::ssize(rStates) > iCpuTimer &&` term re-checks an invariant. Entries are inserted into `mPerThreadTimerStates` only by `try_emplace` (~161, ~200), each followed under the same `mCpuTimerMutex` by a resize to `miCpuTimerCount` (~162-165, ~201-206). Nothing shrinks the vectors, and `miCpuTimerCount` is fixed in the constructor (~32). Simpler form: drop the term. `.at()` still bounds-checks.
23. **`ProfileManagerBase` `kCpuTimerAcquireToGlobal` search (~701).** The `std::ssize(rStates) > kCpuTimerAcquireToGlobal &&` term relies on the same invariant (`miCpuTimerCount >= kEngineCpuTimerCount > kCpuTimerAcquireToGlobal`). The comment's "missing state is valid" case concerns a zero `startTimePoint`, or a thread with no map entry, and both stay handled. Simpler form: drop the term.

**Ui**

24. **`Ui/Screens/TweaksScreen/TweaksSliderMap.cpp` (~28-32).** `common::Assert(bInserted, ...)` sits inside `if (!bInserted)`, so its condition argument is always false. Simpler form: `common::Assert(false, std::format("Duplicate Tweaks slider key: \"{}\"", rEntry.first));`. Keep the surrounding `if`, so `std::format` runs only on a duplicate. The gain is cosmetic.

## Critical files

- `Engine/Source/Audio/StaticVoices.h`, `Engine/Source/Audio/StaticVoices.cpp`, `Engine/Source/Audio/AudioManager.h`, `Engine/Source/Audio/AudioManager.cpp`, `Engine/Source/Audio/StreamingVoices.cpp`
- `Engine/Source/CrashReport.cpp`, `Engine/Source/LaunchOptions.cpp`
- `Engine/Source/Frame/Collision.cpp`, `Engine/Source/Frame/IslandChainPlacement.cpp`, `Engine/Source/Frame/IslandTerrain.cpp`, `Engine/Source/Frame/NavBuild.cpp`, `Engine/Source/Frame/NavQuery.cpp`
- `Engine/Source/Input/RawInputManager.h`, `Engine/Source/Input/RawInputManager.cpp`
- `Engine/Source/Profile/ProfileManagerBase.h`, `Engine/Source/Profile/ProfileManagerBase.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.cpp`
- Call sites: `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp` (the `PlayOneShot` call) and `Engine/Source/Graphics/CameraBase.cpp` (the `SetVibration` call)

## In scope

- Audio:
  - the 2D `PlayOneShot` signature in `StaticVoices` and `AudioManager`, and its one `PlayersCombat.cpp` call;
  - `StreamingVoices::Clear`: the `mPreviousStreams` null check and both `mStreamsToDestroy` loops.
- `CrashReport.cpp`: the `wcscpy_s` result branch.
- `LaunchOptions.cpp`: the two `pcEnd` null terms in `--windowed` parsing.
- Frame:
  - `TestAndCollectPair`'s `kAlreadyCollided` block;
  - `OrientForTangent`'s `fShort <= 0.0f` term;
  - the server NavContour loop's `puiHeightmapHalf` wrapper;
  - `BuildNavContour`'s CCW-assert-loop `iCount < 3` skip, and its `Lerp` clamp;
  - `AStarPath`'s walk-back loop condition.
- Input:
  - the `RawInputManager` constructor's `catch (...)` handler;
  - the `SetVibration` signature and its one `CameraBase.cpp` call.
- Profile:
  - `LatchRawCpuTimer` and `ArmRawCpuTimerEvent`, each definition and declaration;
  - the `iEventSequence == 0` term in `AcknowledgeRawCpuTimerEvent`;
  - the two `std::ssize(rStates) >` terms.
- Ui: the `common::Assert` argument in `TweaksSliderMap.cpp`.

## Out of scope

- **Do not change (rejected by validation):** `Engine/Source/Frame/CellStaticData.cpp` (~62-64), the loop-indexed `islands.at(i)` and `islandRenderQueries.at(i)`. `Documents/C++StyleGuide.txt` rule 16 requires `.at()` for `std::vector` access; it is style, not defensive over-engineering.
- Every `if (gpX == this)` singleton teardown guard, including the former candidates 14 and 16 and the `AudioManager.cpp` and `IslandTerrain.cpp` ones: `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles the whole family once.
- `Engine/Source/Network`, `Server`, `File`, and `Graphics` candidates, apart from the one `CameraBase.cpp` `SetVibration` call-site update.
- The `mStreamsToDestroy` member and its `StreamingVoices::Update` producer and consumer, the `mStreamsToDestroy.clear()` in `Clear`, and its count term elsewhere in `StreamingVoices.cpp`.
- `StreamingVoices::Update`'s `AllowOlderFadeRequests` call site, which `Documents/Plans/Engine/RemoveAgentOverEngineering.md` owns.
- `PlayOneShotLocked`, `PlayOneShotThreeDimensional`, and `PlayOneShot3d`; `ArmRawCpuTimerEventLocked` and `LatchRawCpuTimers`; every other `ProfileManagerBase` member.
- The user-input validation in `LaunchOptions.cpp`.
- Every other collision, island, and navigation computation.
- Any candidate the executor cannot prove. Report it; do not substitute a different change for it.
- Every boundary listed under `### Boundaries that are never removed`.

## Acceptance criteria

- Every candidate is either applied with the executor's own cited proof (the invariant or every caller, plus a bit-identity argument for candidates 8-13) or dropped and reported with the `path:line` that makes it reachable.
- Client and server compile in Debug and Release with no new warnings.
- `/agent-harness`:
  - A replay determinism check passes with no CRC mismatch, over a session with ship-missile-player combat (collision), unit pathing (nav query), and island cells with chain placement and NavContour builds.
  - The shield-down one-shot plays.
  - Music streaming survives the harness rig's `Clear` paths (`audio_streaming_harness_rig` `start` and shutdown).
  - The client launches with a valid `--windowed WxH`, and invalid ones (`--windowed 0x0`, `--windowed 800`, `--windowed 800x600z`) are still rejected with the existing error log.
  - The Tweaks screen opens.
  - The server raw CPU timer harness rig's arm/acknowledge sequence reports as before.
- The executor states that gamepad vibration (candidates 17-18) is compile-verified only when no gamepad is available to the harness.

## Notes

- Change Workflow tier: **Tier 3**. Two triggers apply. The first is edits to deterministic simulation code on the per-tick CRC surface (`TestAndCollectPair`, `OrientForTangent`, `BuildNavContour`, `AStarPath`): a mistaken equivalence proof would change CRC-checked PostRender state. The second is that the change spans independently owned subsystems (Audio, Frame, Input, Profile, Ui, plus a `Projects/` call site and a `Graphics` call site). The Audio, Input, Profile, Ui, crash report, and launch option candidates alone carry no determinism, wire, serialization, threading, or trust-boundary exposure. The `ProfileManagerBase` edits keep the existing `mCpuTimerMutex` locking unchanged.
- No `AGENTS.md` or documentation names any symbol this Plan removes.
- Sibling sweep Plans: `Documents/Plans/Engine/RemoveCommonOverEngineering.md` and `Documents/Plans/Engine/RemoveAgentOverEngineering.md`. They have no ordering constraint with this Plan. The Agent Plan edits only the `StreamingVoices::Update` call site, while this Plan edits only `StreamingVoices::Clear`.
- Separately, `Documents/Plans/Engine/GraphicsOverEngineeringCleanup.md` (candidate 2) edits another region of `CameraBase.cpp`, and `Documents/Plans/Game/RemoveSandboxOverEngineering.md` (S23, S24) edits other regions of `PlayersCombat.cpp`. This Plan touches one call statement in each.
