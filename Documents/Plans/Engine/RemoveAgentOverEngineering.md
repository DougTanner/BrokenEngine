<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:17:42.169Z","dependsOn":[]} -->
# Remove over-engineered checks and fallbacks from the agent command channel

## Context

A repository-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, ultra-rare edge-case protection, and speculative generality that should be a plain operation plus an `ASSERT` or a hard error. A low-effort finder model flagged candidates per file, and one validation pass by a stronger model confirmed or rejected each against the code. The main session spot-checked only two validation groups, and when this Plan was written, a check found every candidate below still present in the source. The sweep's working files were temporary and are gone. This Plan carries the `Engine/Source/Agent/**` results inline: 11 of the 12 confirmed candidates and 1 rejection. The twelfth, candidate 5 (the `~AgentUiRegistry` teardown guard), moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`; the remaining candidates keep their original numbers.

The agent command channel is a developer tool and its input is trusted (`Engine/Source/Agent/AGENTS.md` `## Architecture`). A check on a state the channel itself guarantees is therefore a valid candidate.

**Recorded user direction (relayed by the dispatching session):** the candidates below must not be trusted blindly. The executing agent re-verifies each one independently against current code; line numbers have drifted, so locate each by symbol. It applies only a candidate it can prove itself, citing the invariant or every caller, and it drops and reports any candidate that does not hold. Each candidate is a proposal, not an approved change.

## Design

### Re-verification protocol (per candidate)

1. Locate the code by symbol. Confirm that the quoted check or function still exists.
2. Re-derive the validator's evidence yourself: every caller (`git grep` outside `ThirdParty/`; for ImGui hook call sites, Grep the explicit `ThirdParty/imgui/` file paths, because Glob and directory Grep skip submodule links), every writer of the guarded state, and the invariant that makes the case impossible. If any caller, writer, or path makes the case reachable, drop the candidate and report it with that `path:line`.
3. Apply the simpler form. The `cpp-conventions.md` "No useless ASSERTs" rule decides whether to replace a check with an `ASSERT` or delete it outright. If the very next statement would dereference the same pointer, or the same index would be bounds-checked by `.at()`, delete the check without an `ASSERT`. If the impossible case would otherwise be undefined behavior without a guaranteed fault (`back()` on an empty vector, `erase(end())`), use `ASSERT(<cond>)`. The recommended forms below follow this rule, so they differ from the validator's suggestion in a few places.
4. If a candidate's proof needs code outside `## In scope`, drop it and report it. Do not widen scope.

### Boundaries that are never removed

- Server validation of every client-to-server record and every grid save. A bad value read from a packet or file is rejected and never clamped or substituted (`.agents/references/cpp-conventions.md` bad-value rule).
- The per-tick determinism CRC and anything that feeds it.
- `.pack`/manifest/save/replay format and version checks.
- Win32 (including Winsock) and Vulkan API result checks on calls that remain, and checks on external input: user files, third-party tool output such as the RenderDoc API, and OS state the program does not control.

Converting a check into an `ASSERT` is an acceptable outcome.

### Candidates (validator evidence in brief)

1. **`AgentCommandServer.cpp` listener loop after `ServeConnection` (~198-202).** The `muiActiveSocket != INVALID_SOCKET` guard is always true. The only writes are the accept assignment (~184), which takes a valid socket (the stop path at ~179-183 breaks first, and the invalid-accept paths at ~148-165 `continue` or `break` earlier), and this block. `ServeConnection` and the destructor do not write it. Recommended form: under the existing lock, call `closesocket(muiActiveSocket); muiActiveSocket = INVALID_SOCKET;` unconditionally.
2. **`AgentCommandServer.cpp` `~AgentCommandServer` (~109-113).** The `muiListenSocket != INVALID_SOCKET` guard is always true. Every constructor path that leaves the socket invalid throws (~35-39, ~54-58, ~67-73, ~76-82), so the destructor never runs for such an object. `ListenerLoop` only reads the socket (~142, ~147). Recommended form: an unconditional `closesocket(muiListenSocket); muiListenSocket = INVALID_SOCKET;`. The `ListenerLoop` check at ~142 stays, because the destructor makes that case reachable.
3. **`AgentInput.cpp` `AgentInput::StabilizeTarget` (~114-118).** The null `gpAgentUiRegistry` branch cannot happen. `AgentUiRegistry` and `AgentInput` are created together, registry first, in the `iAgentPort != 0` block (`Engine/Source/Main.cpp` ~186-187). The registry is declared first (~162-163), so it is destroyed after `AgentInput`. `StabilizeTarget` runs only from `AdvanceFrame` (`AgentInput.cpp` ~200, ~239, ~259), which is reached through `gpAgentInput` (`Main.cpp` ~412-414). Recommended form: delete the branch without an `ASSERT`, because the next statement dereferences `gpAgentUiRegistry`.
4. **`AgentUiRegistry.cpp` `CopyTruncate` (~13-17).** The null-source branch is unreachable. Every source is non-null:
   - the hook's window name, or `""` (~305), and `Swap`'s `pWindow->Name` (~218);
   - internal pending-label buffers (~119);
   - `pcLabel`, guarded at ~135 and ~144 before ~160;
   - `pcValue`, a local `snprintf` buffer (`Engine/Source/Ui/MenuUtils.cpp` ~72-74, ~154-157).

   Recommended form: delete the branch without an `ASSERT` (the loop dereferences `pcSource` next), and remove "pcSource may be null." from the function comment.
5. **Moved.** The `~AgentUiRegistry` teardown guard moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`.
6. **`AgentUiRegistry.cpp` `AgentUiRegistry::Swap` (~205-209).** The null ImGui context check and the null entries check in `pContext->Windows` cannot happen. The only caller is `Engine/Source/Graphics/Managers/ImGuiManager.cpp` (~511), right after `ImGui::Render()` (~505), which needs a current context. ImGui adds windows only by pushing a newly created window (`ThirdParty/imgui/imgui.cpp` ~6630, ~6632). Recommended form: remove the `if (pContext != nullptr)` wrapper without an `ASSERT`, and change the skip test to `if (!pWindow->WasActive)`.
7. **`AgentUiRegistry.cpp` ImGui ItemAdd hook (~305-308).** The null `pContext->CurrentWindow` handling is unreachable. Every hook call site already has the current window set or uses it:
   - `ThirdParty/imgui/imgui.cpp` ~7764 and ~8062, inside `Begin` after `SetCurrentWindow` (~7591);
   - `ItemAdd` (~11243), which reads `g.CurrentWindow` at ~11198;
   - `ThirdParty/imgui/imgui_widgets.cpp` ~569, which uses `window->RootWindow` at ~563.

   Recommended form: `const char* pcWindow = pContext->CurrentWindow->Name;` and `bool bVisible = rBoundingBox.Overlaps(pContext->CurrentWindow->ClipRect);`, with no `ASSERT` (the next statement dereferences `CurrentWindow`). Keep the registry null check.
8. **`Commands/AudioStreamingHarnessRig.cpp` `AllowOlderFadeRequests` (~831-834).** The function always returns `false`, reads no state, and has one use: `Engine/Source/Audio/StreamingVoices.cpp` ~90, under `#if defined(BT_DEBUG)`, whose `#else` passes the same `false`. Simpler form: delete the function and its declaration (`AudioStreamingHarnessRig.h` ~223), and replace the `#if`/`#else` block in `StreamingVoices::Update` (~89-93) with `mPreviousStreams.at(i)->UpdateRequests(false);`.
9. **`Commands/AudioStreamingHarnessRig.cpp` `RetireVoiceControl` (~870-873).** The missing-control branch is unreachable. A voice gets a control only from `CreateVoiceControl` (`StreamingVoices.cpp` ~190), which always adds it to the attached rig's list (~836-852). The only other removal, `ReleaseControls` (~223-234), first nulls every voice's `mpAudioStreamingControl`, so retire then returns early. A missing control could only come from a rig detached without `Shutdown`, which would already leave a dangling pointer. Recommended form: `ASSERT(it != pHarnessRig->mVoiceControls.end());` then an unconditional `erase(it)`.
10. **`Commands/ClientNetworkHarnessRigs.cpp` `PollBeforeDrain` (~117-120).** The slot-index range re-check cannot fail. `iSlotIndex` is set only in `CaptureStaleUpdate` (~83), which already indexes `mCoordinateSlots.at(iSlotIndex)` (~86) for the same bound client (~72, ~95). `mCoordinateSlots` is resized once, in the `Client` constructor (`Engine/Source/Network/Client/Client.cpp` ~29), and is never cleared. Recommended form: delete the check. The `.at()` on the next statement still bounds-checks.
11. **`Commands/ReplayHarnessRigs.cpp` `DropRetainedEndFrame` (~105-108).** The empty writer-generation list is unreachable. `mReplayWriters` entries are created only by `try_emplace`, and a generation is always added right after (`Engine/Source/File/Replay.cpp` ~331-338 and ~803-807). Nothing pops or erases a generation; the only removal is the map-wide `clear()`. Recommended form: `ASSERT(!it->second.empty());` (the following `back()` would be undefined behavior).
12. **`Commands/ReplayHarnessRigs.cpp` `ArmPersistenceFailure` (~139-142).** Same invariant as candidate 11. Recommended form: `ASSERT(!it->second.empty());`.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.cpp`
- `Engine/Source/Agent/AgentInput.cpp`
- `Engine/Source/Agent/AgentUiRegistry.cpp`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
- `Engine/Source/Agent/Commands/ClientNetworkHarnessRigs.cpp`
- `Engine/Source/Agent/Commands/ReplayHarnessRigs.cpp`
- `Engine/Source/Audio/StreamingVoices.cpp` (one call site)

## In scope

- `AgentCommandServer`: the destructor's listen-socket close, and the active-socket close after `ServeConnection` in the listener loop.
- `AgentInput::StabilizeTarget`: the registry null branch only.
- `AgentUiRegistry.cpp`: `CopyTruncate` and its comment, `Swap`'s context and window-entry checks, and the ItemAdd hook's `CurrentWindow` handling.
- `AudioStreamingHarnessRig::AllowOlderFadeRequests` (definition and declaration), the `#if defined(BT_DEBUG)` block that calls it in `StreamingVoices::Update`, and the missing-control branch in `RetireVoiceControl`.
- `PollBeforeDrain`: the slot-index range check only.
- `DropRetainedEndFrame` and `ArmPersistenceFailure`: the empty-generation branch only.

## Out of scope

- **Do not change (rejected by validation):** `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` (~361-366), the `GetCapture(...) == 0` check and the `uiPathLength > 0` ternary in the RenderDoc capture path. Both check the output of the third-party RenderDoc API, which the program does not control, and they report a clear error instead of building an empty path.
- The `ListenerLoop` listen-socket check (~142), which the destructor makes reachable.
- The `~AgentUiRegistry` and `~AgentInput` teardown guards: `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles the singleton teardown guard family once.
- Every other agent command, harness rig, and `AgentUiRegistry` member. In particular, the unsigned counters of `AudioStreamingHarnessRig` belong to `Documents/Plans/Engine/AudioStreamingHarnessRigSignedCounters.md`, which edits other regions of the same file.
- `StreamingVoices.cpp` beyond the one call site named in `## In scope`. Its `Clear` cleanup belongs to `Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md`.
- Any candidate the executor cannot prove. Report it; do not substitute a different change for it.
- Every boundary listed under `### Boundaries that are never removed`.

## Acceptance criteria

- Every candidate is either applied with the executor's own cited proof (the invariant or every caller) or dropped and reported with the `path:line` that makes it reachable.
- Client and server compile in Debug and Release with no new warnings.
- `/agent-harness`:
  - a launched client and server accept agent connections, and both shut down cleanly (destructor paths);
  - a UI label click script resolves and clicks its target (`StabilizeTarget`, `CopyTruncate`, `Swap`, and the hook);
  - `audio_streaming_harness_rig` `start`, `inspect`, and shutdown run as before;
  - the replay and client-network harness rig actions that reach `DropRetainedEndFrame`, `ArmPersistenceFailure`, and `PollBeforeDrain` report the same results as before.

## Notes

- Change Workflow tier: **Tier 2**. The trigger is scoped runtime behavior of one subsystem, the developer-only agent command channel, plus one Audio call site whose argument value is unchanged. The socket closes keep their existing lock and ordering, so there is no threading change. There is no determinism/CRC, wire, serialization, or trust-boundary exposure: the agent channel is a trusted developer tool.
- No `AGENTS.md` or documentation names any symbol this Plan removes.
- Sibling sweep Plans: `Documents/Plans/Engine/RemoveCommonOverEngineering.md` and `Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md`. Both are independent of this Plan. The Engine runtime Plan also edits `StreamingVoices.cpp`, but only `StreamingVoices::Clear`, while this Plan edits only the `Update` call site, so there is no ordering constraint.
