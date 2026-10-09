<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:18:31.000Z","dependsOn":[]} -->
# Remove over-engineered checks in the BrokenEngineSandbox game

## Context

A repository-wide over-engineering sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, and ultra-rare edge-case protection that a plain operation plus an `ASSERT` or a hard error would replace. A low-effort first-pass model produced the findings; a second model validated each one once against the code (about 95% confirmed), and the dispatching session spot-checked only two of the sweep's 27 groups. For `Projects/BrokenEngineSandbox/`, 36 findings survived that validation. This Plan lists 34 of them as candidates (S1-S34). The other two are under "Do not change", along with the one rejected entry.

User direction, as relayed by the dispatching session: do not trust these recommendations blindly. Every listed finding is a candidate, not an approved change. Line numbers were taken on 2026-10-08 and will drift.

## Design

Recommended procedure for the executing session:

1. For each candidate below, find the current code by symbol, not by line number. Re-derive the claim yourself by reading every caller and the invariant it relies on.
2. Apply a candidate only when you can prove it from current code: cite the invariant or every caller in the change summary. Drop any candidate that does not hold, whose proof needs more than a local reading, or whose code has changed shape. Report each dropped one with the reason.
3. The simpler forms are deletion, `ASSERT(...)`, or a hard error. `ASSERT` is always active (`Common/ErrorUtils.h:20`), so turning a check into an `ASSERT` keeps a hard stop and is an acceptable result.
4. Return a per-ID table: applied (with its proof) or dropped (with its reason).

Boundaries that must never be removed, even when a candidate seems to propose it:

- Server validation of every client-to-server record and every grid save, under the bad-value rule in `.agents/references/cpp-conventions.md` (reject the input; never clamp or substitute a value). Owners: `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` and `Engine/Source/File/AGENTS.md` `## Grid Saves`.
- The per-tick determinism CRC and anything that feeds it. Several candidates sit in PostRender or server simulation code (S18-S24, S32). Each must leave PostRender state and every random draw bit-identical, which is why the Players change keeps the `Random` draw unconditional.
- `.pack`, save, and replay format and version checks.
- Win32 and Vulkan result checks, and checks on genuinely external input.

What may be trusted: the client trusts server-to-client data, and replays and the agent command channel are developer tools (root `AGENTS.md`, trust policy). That is why the agent-command and harness-rig candidates are eligible.

### Candidates

Agent (developer tooling)

- **S1** `Source/Agent/AgentScene.cpp:100` `if (bFocused && pFocused != nullptr)`: `bFocused` means `i == miFocusedFleetIndex` within range (:91-96), and under that condition `FleetSelection::FocusedFleet()` returns non-null (FleetSelection.cpp:48-55). Both are read in the same main-thread call. Form: `if (bFocused)`.
- **S2-S6** `Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:480-487` (`start`), `:541-548` (`release_read`), `:579-586` (`coexistence`), `:627-630` (`invalid`), `:669-676` (`saturate`): the deferred polls check for a null `gpFileManager` and/or `gpAudioManager`. Polls run only from `AgentCommandServer::Drain`. `pFileManager` (Main.cpp:891) outlives `MainThread`. `pAudioManager` (Main.cpp:147) is declared before `pAgentCommandServer` and its clearing scope guard (Main.cpp:154-158), so the server global is cleared before the audio manager dies. `~AgentCommandServer` (AgentCommandServer.cpp:98) and `Game::~Game` (Game.cpp:358) both clear the poll, and each handler checks both managers on entry (:446-453). Form: delete each block.
- **S7** `ClientFullStateHarnessRig.cpp:27` `sHarnessRig.pSession != nullptr` is redundant once `bArmed` is set: the only `bArmed = true` (:296) sets `pSession` to an already-dereferenced session in the same statement, and `sHarnessRig = {}` (:33) resets both. Form: `return sHarnessRig.bArmed && sHarnessRig.pSession->mpDesynchronizationCore.get() == &rCore;`.
- **S8** `ClientFullStateHarnessRig.cpp:225-228` `!rParameters.is_object()` repeats the following `contains("action")` check, because nlohmann's `contains` is `is_object() && find(...)` (bundled json.hpp). The error message is the same. Form: delete.
- **S9** `ClientPacketFaultHarnessRig.cpp:96-99` `if (pSession == nullptr) return;` in `InjectArmedClientPacketFault`: after the :84 early return the rig is armed, and `sArmedPacketFault`/`spSession` are set together (:77-78, after the :58 null check) and cleared together (:91-94, :115-116). Form: `ASSERT(pSession != nullptr);`.
- **S10** `ClientPacketFaultHarnessRig.cpp:104-107` `mpClient == nullptr` cannot occur while armed. Arming requires a connected client (:62-69), `ClientSessionRuntime::Disconnect` (Engine/Source/Network/Client/ClientSessionRuntime.cpp:152,159) resets the rig via `OnRuntimeDisconnected` (ClientSession.cpp:272), `Connect` (:133) installs a non-null client, and the session destructor resets the rig (ClientSession.cpp:49). Form: `ASSERT(pSession->mpRuntime->mpClient != nullptr);`.
- **S11** `ServerFaultHarnessRigs.cpp:163-166` `if (pPeer == nullptr) throw` after the exactly-one-handshaken-client check. `pPeer` has one assignment, from the ENet event peer in `Server::Connect` (Engine/Source/Network/Server/Server.cpp:177), and is never cleared. Form: delete, or `ASSERT(pPeer != nullptr);`.
- **S12** `ServerFaultHarnessRigs.cpp:217-220` `if (pClient == nullptr) throw` right after the `iHandshakenClientCount != 1` throw: a count of 1 means :209 assigned it. Form: delete.
- **S13** `ServerFaultHarnessRigs.cpp:221-224` `if (pClient->pPeer == nullptr) throw` on a handshaken connection: same invariant as S11. Form: delete, or `ASSERT(pClient->pPeer != nullptr);`.
- **S14** `ServerFaultHarnessRigs.cpp:260-263` a runtime throw when a default `ClientAckStreamMessage` serializes to anything other than `kiFixedSize`. With `uiSlotCount = 0` (:256), `Visit` (Engine/Source/Network/NetworkMessages.h:444-459) writes only the type, count, and timestamp, which is exactly `kiFixedSize` (:433). This is a compile-time-determined invariant. Form: `ASSERT(iPacketSize == engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize);`.
- **S15** `ServerSimulationHarnessRigs.cpp:680-683` the `else if (gpGame->mbReplaying)` throw is unreachable: :646 already throws on it, and nothing in between changes it. Form: delete.
- **S16** `ServerSimulationHarnessRigs.cpp:878-885` `QueueReplayTransferHarnessRig`'s `IsTransferType` and `holds_alternative<TransferData>` early returns repeat what its only caller (:807) already guarantees: it calls only under `IsTransferType`, and `BuildInjectedEntry` reads `std::get<TransferData>` (:439). Declared at ServerSimulationHarnessRigs.h:29 with no other caller. Form: `ASSERT(IsTransferType(transfer.eType) && std::holds_alternative<TransferData>(transfer.data));`.

Fleet selection and frame collections (S18-S24 are simulation code)

- **S17** `Source/FleetSelection.cpp:202` `focusedIt != pFleet->members.end() &&` is redundant when `mFocusedMemberGlobalId` is non-zero: :187 resets it unless it is a member, and :194/:198 assign only `members.back().globalPlayerId`. This is client-only UI state. Form: drop the clause, optionally `ASSERT` it inside the branch.
- **S18** `Source/Frame/Collections/Blasters/Blasters.cpp:55-69` two consecutive `if (pfWindTrailIntensities[iIndex] > 0.0f)` blocks test the same unchanged value. `WindTrailsPostRender::Add` (Engine/Source/Frame/Collections/WindTrails/WindTrailsUpdate.cpp:27) does not touch blaster intensities, and the mirrored site Players.cpp:366-367 already does `Add` and `Sync` under one check. Form: merge into one block, with `Sync` after `Add`.
- **S19** `Missiles/Missiles.cpp:435-443` `Fall`'s `kExploding`/`kFalling` early returns repeat the only caller's guard (MissilesUpdate.cpp:230-234, same row; the intervening `ReleaseRegistryTarget` does not touch flags). Form: `ASSERT(!(rCurrentPostRender.pFlags[i] & (kExploding | kFalling)));`, or two `ASSERT`s if the flag type has no combined mask.
- **S20** `Missiles/Missiles.cpp:476-479` `Explode`'s `kExploding` early return repeats the PostCollision loop's skip (MissilesUpdate.cpp:366-369). Both callers (:385, :403) are followed by `continue` or the loop end. Form: `ASSERT(!(rCurrentPostRender.pFlags[i] & kExploding));`.
- **S21** `Players/Players.cpp:435-437` the `rInfo.fCellChangeTimer > 0.0f` branch is dead: the `SpawnInfo` producers (Players.cpp:270, SpawnTransfer.cpp:79) never set it, and it defaults to 0 (Players.h:335). Form: delete the `SpawnInfo::fCellChangeTimer` field and assign `15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine)` directly. The draw stays unconditional. Also drop the dead legacy-check mention at `Source/Frame/Collections/AGENTS.md:13` (and in `Players/AGENTS.md` if present).
- **S22** `Players/Players.cpp:388`, `Players.h:346` `PlayersPostRender::Spawn(Frame&, const SpawnInfo&)` always returns `true` (:456), and both callers (:270, SpawnTransfer.cpp:79) discard the result. Form: `static void Spawn(...)`.
- **S23** `Players/PlayersCombat.cpp:70,72,391` the file-static `FindTargetSpaceshipIndex`'s `fMaximumRange` only ever receives `kfMissileTargetRange` (:25, :391). Form: drop the parameter and use the constant at :72.
- **S24** `Players/PlayersCombat.cpp:159,177-178` the file-static `ApplyDamage`'s defaulted `fHexShieldIntensity = 1.0f` is never passed (:238, :242). Form: drop the parameter and write `1.0f` at :177-178.
- **S25** `Spaceships/Spaceships.cpp:155-158` the `RegisterEnemyBlasterType` guard `siEnemyBlasterTypeIndex != 0xFF`: the only caller, `SpaceshipsInterpolate::Register` (:144), runs once from `GameBase::GameBase()` (GameBase.cpp:24). A repeat call would already fail `ASSERT(riIndex == kiInvalidTypeIndex)` (Engine/Source/Frame/Collections/Collection.h:134) at :109, and `BlastersInterpolate::RegisterType` asserts the same. Form: delete.
- **S26** `Spaceships/Spaceships.cpp:196-197` the `if (siSpaceshipHitFlashTypeIndex == 0xFF)` wrapper in `RegisterSpaceshipHitFlashEffect`: same once-only chain (:146), and `RegisterType` asserts. Form: remove the `if` and un-indent.
- **S27** `Source/Frame/ServerCellStats.cpp:52-67` four `if constexpr (kbProfiling)` wrappers repeat the `!kbProfiling` early return (:33-36). Form: four plain assignments.

Game, client, and server

- **S28** `Source/Game.cpp:523-525` the `kGame` clause of the `ChangeFrame` re-entry guard can never match. Both callers (Game.cpp:511, Network/Client/ClientSession.cpp:227) pass `GameFlags::kMainMenu`, and the engine reaches it only through `ApplyStandardMenuAction(kChangeFrameToMainMenu)` (Engine/Source/Ui/Screens/PauseMenuScreen.cpp:71). The `StartGameMusic()` else arm (:533-537) is dead as a result. Form: `ASSERT(!(gameFlags & GameFlags::kGame));` at the top, reduce the guard to the `kMainMenu` clause, and drop the dead else arm.
- **S29** `Source/Game.cpp:662-666` `CaptureClientStateIfChanged`'s early return only skips three assignments that would write the same values (:668-670), and the function returns `void`. Form: delete the block. Optionally rename the function to drop `IfChanged`, updating its callers.
- **S30** `Source/Network/Client/ClientSession.cpp:240-241` `static_assert(kiTickCounter >= 0)` checks a local `constexpr` 0 that has one use. Form: delete it and write `gpGame->miTickCounter = 0;`.
- **S31** `Source/Network/Client/ReconcileReplayClientState.cpp:101` (helper :12-51) the file-static `FindMatchingPlayerInCoordinate`'s result is discarded at its only call. It writes no state, and its only side effect is `kVerbose` logging (:43, :47). Form: delete the call and the helper.
- **S32** `Source/Network/Server/FleetNavigationController.cpp:22` the `default` arm of `NavigationDirectionOffset` is unreachable: its only caller (:92) passes `common::Random(3i64, rRandom)`, whose range is 0..3 (Common/Math/Random.cpp:56-61). This is server simulation code. The fallback never runs, so replacing it does not change the random draws or the result. Form: `default: std::unreachable();` (the repository's existing idiom) or `ASSERT(false); return {};`.
- **S33** `Source/Network/Server/ServerClientManager.h:24` the `= {}` defaults on `rFleetGuid` and `memberGlobalPlayerId` are never used: all three callers (ServerFleetManager.cpp:149, 189, 231) pass both arguments. Form: drop both defaults.
- **S34** `Source/Network/Server/ServerSession.cpp:61-64` the `contains` check before `try_emplace` repeats it. Game.cpp:312 already uses the one-line form. Form: `gpGame->mFrameInputs.try_emplace(rCoordinate);`.

### Do not change

- `Source/Network/Server/ServerFleetSerialization.cpp:116-118` `.at(k)`: `Documents/C++StyleGuide.txt` rule 16 (`:130`) requires `.at()`, and the save validation around it is a required trust boundary.
- `Source/Network/Client/ClientSession.cpp:53-56` and `Source/Profile/ProfileManager.cpp:44-47` (`if (gpX == this)` destructor teardown): `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles every site of this singleton teardown guard family, these two included, once, so the root `AGENTS.md` mirrored-pattern directive is met in one change.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`, `ClientFullStateHarnessRig.cpp`, `ClientPacketFaultHarnessRig.cpp`, `ServerFaultHarnessRigs.cpp`, `ServerSimulationHarnessRigs.cpp` (+ `.h`)
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`, `Source/Frame/ServerCellStats.cpp`, `Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp`, `Missiles/Missiles.cpp`, `Players/Players.cpp`, `Players/Players.h`, `Players/PlayersCombat.cpp`, `Spaceships/Spaceships.cpp`, `Source/Frame/Collections/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`, `ReconcileReplayClientState.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`, `ServerClientManager.h`, `ServerSession.cpp`

## In scope

- Only the code regions named by candidates S1-S34, each changed only to the stated simpler form or an equivalent `ASSERT`/hard error, and only after the executor's own proof.
- Removing a field, parameter, return value, or file-static helper that a candidate leaves unused (S21, S22, S23, S24, S31, S33), with its call sites.
- The `AGENTS.md` sentence that S21 makes false, and any comment adjacent to a changed region that the change makes false.

## Out of scope

- Every boundary in `## Design`, the "Do not change" list, and any candidate the executor cannot prove.
- Any change to PostRender state, a random draw, the CRC, wire messages, save or replay layout, or threading. If a candidate turns out to need one, drop it.
- `Engine/`, `Common/`, and `DataPacker/`.
- Every `if (gpX == this)` singleton teardown guard, including the `Game.cpp`, `ClientSession.cpp`, and `ProfileManager.cpp` ones: `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles the whole family once.
- New findings beyond S1-S34. Report them instead of applying them.

## Acceptance criteria

- Client and server build through `/compile` (Shared data mode) with no new warnings.
- The change summary lists every ID S1-S34 as applied (with the cited invariant or callers) or dropped (with the reason).
- An `/agent-harness` replay determinism check passes on the changed server, showing that the simulation candidates (S18-S24, S32) leave the per-tick CRC unchanged.
- No boundary in `## Design` is weakened.

## Notes

- Change Workflow tier: Tier 2. Trigger: scoped behavior made of local, behavior-preserving check removals across game-side code. The intended change does not alter what the CRC computes, what the wire carries, or what the server trusts. A reviewer should escalate to Tier 3 if any applied candidate changes PostRender computation or a random draw.
- Other Plans edit some of the same files (`Documents/Plans/Game/CollectionTimesToChrono.md`, `ControllerTypeIndexStaticsToInt64.md`, `ExplosionParticleCountsToInt64.md`, `FrameEditRows.md`). None shares a root cause with this Plan. Re-verifying against current code covers whichever lands first. `FrameEditRows.md` calls `PlayersPostRender::Spawn` without using its result and relies on the unconditional `Random` draw, so S21 and S22 stay compatible with it.
- `Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md` (candidate 1) also edits `PlayersCombat.cpp`: only the `PlayOneShot` call (~`:195`), a region disjoint from S23 and S24, so there is no required ordering.
