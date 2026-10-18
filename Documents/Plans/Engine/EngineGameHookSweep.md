<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T13:05:05.556Z","dependsOn":[]} -->
# Sweep `Engine/` and `Common/` for remaining engine-to-game hooks

Tier 3 (`.agents/references/risk-tiers.md`): fixes may land in any engine
subsystem plus the game sides that implement them, so the change spans
independently owned subsystems. Each fix must preserve behavior;
`## Sweep rules` and the type's `## Fix bound` defer anything that would change
sim output, CRC, serialized bytes, threading, or trust. This Plan runs through
`/sweep` (`/next-plan` routes a Plan with a `## Sweep` section there), not the
Change Workflow. Line numbers cite baseline `45b5e347`. Where a statement here
and the code disagree, the code wins; report the contradiction instead of
matching one side to the other.

## Context

The user's words this session, quoted by the dispatching session: "Now's the
time to do any large refactors, before we create the second game project, NOTE:
Avoid 'hooks' the engine can just call functions/globals it expects the game::
to have available." and, choosing to remove several hooks in the claimed Plan:
"H2, but file a followup plan to sweep the codebase for other 'hooks' to clean
up". Standing constraints relayed with them: engine code may name expected
`game::` classes, globals, functions, and members; engine code must not name
Sandbox gameplay (`Blaster`, `Spaceship`, `Missile`, `Omni`, `bUseMissiles`,
`Fleet`); fleets stay game-owned; no backward compatibility; the server
validates every grid save.

A hook here means engine-to-game indirection that exists only so the engine can call the game: a function pointer, `std::function`, or registered callback the game installs; a virtual function, pure virtual, or virtual destructor in an engine class whose only derived class is a game class; a trait struct, concept, or template parameter that only binds game names; a constructor span or pointer table the game passes only so the engine can reach game storage.

A read-only survey at the baseline covered `Engine/`, `Common/`, and `Projects/BrokenEngineSandbox/`. Its findings, which this sweep must not redo:

- Removed by the claimed Plan above,
  which has landed: the Tweaks extension table
  `TweakExtensionHook`/`kTweakExtensionHooks` and its walkers
  (`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h:35-42`,
  `TweaksScreenBase.cpp:55-87`); `game::TweaksScreen : TweaksScreenBase` and
  `virtual ~TweaksScreenBase` (`TweaksScreenBase.h:70`); the `GameBase` virtuals
  `ShouldUseCrosshair`, `ProcessGameMenuInput`, `GetStandardMenuModel`,
  `ApplyStandardMenuAction`, and `virtual ~GameBase`
  (`Engine/Source/GameBase.h:203-212`); the `CameraBase` virtuals `SunAngle`,
  `IsMainMenuFrame`, `PullTarget`, and its virtual destructor
  (`Engine/Source/Graphics/CameraBase.h:162-191`);
  `StreamingVoices::mGetNextTrack` (`Engine/Source/Audio/StreamingVoices.h:50`).
- Removed by prerequisite Plans written alongside this one, which have landed: the `ProfileManagerBase` span constructor and game `ProfileManager` subclass; the `game::NetworkSessionContract` trait struct and `engine::NetworkSessionContractType` concept.
- Reviewed and kept, with the reason:
  - Tweaks section registry `TweakSectionDescription::pRender` and `RegisterSection` (`TweaksScreen.h:25-33,66,73`), reached from the direct call `game::RegisterGameTweakSections`: a dense startup-assigned index space shared by engine and game sections and iterated generically (toggle bar, windows, slider audit), whose game-controlled count and order fold into the persisted `crcLayout`; a constexpr game table would change only its shape.
  - `TweaksSliderMapRegistrar` static-initialization registry (`Engine/Source/Ui/TweaksSliderMap.h:17-20`, `TweaksSliderMap.cpp:18`): a data registry co-located with each slider; it carries data, not calls into the game.
  - Visual-effect type registries `Collection::RegisterType` and `RegisterControllerType` (`Engine/Source/Frame/Collections/Collection.h:132`, `CollectionController.h:116`): a data registry whose type indices the game assigns and orders, reach saved and CRC'd rows, and are validated on grid-save read.
  - Opt-in collection phase calls through `requires` over type lists (`Engine/Source/Frame/FrameUtils.h:197-289`): compile-time per-type dispatch, already direct calls.
- Not game hooks (engine-internal or ThirdParty-facing):
  `Client::GuidAssignedCallback` (`Engine/Source/Network/Client/Client.h:57`,
  only target is engine-static); the Vulkan debug messenger
  (`InstanceManager.cpp`); the ENet `freeCallback` (`NetworkManager.h`);
  per-call `std::function` data writers on `Buffer`, `Texture`, and
  `TextureManager::CreatePlaceholderTexture` (`Buffer.h:66,69`,
  `Texture.h:96-114`, `TextureManager.h:49`); the ImGui test-engine hooks
  (`AgentUiRegistry.cpp`); the ImGui WndProc (`Main.cpp`);
  `common::StackWalker::Emit` and its `Common/StackWalker.h` subclasses.
- Already direct (no change): about 71 `game::` names across 108 engine files, for example `game::WriteReplayMetadata` (`Engine/Source/File/Replay.cpp`), `game::RegisterGameTweakSections` (`Main.cpp`), `game::ExecuteAgentCommand`, and the non-virtual `game::Game` members `GameBase` calls.

At the baseline a text search of `Engine/` and `Common/` for `virtual`, `std::function`, function-pointer declarations, and `concept` found no instance outside the lists above. The sweep therefore confirms, file by file, the tree left after the prerequisite series, which adds new direct game calls and could leave or add indirection.

## Design

Run `/sweep` with the `cpp-adoption` type over the `## Sweep` targets. Each unit's FIND applies `## Sweep rules`; local fixes apply in the unit, and a fix that also edits the game side or other engine files is `cross-file` and is applied by the batch propagation, which updates every caller, override, and doc reference. Deferred findings reach `Documents/Investigations/ChangeWorkflow/EngineGameHookSweepDeferredFixes.md` for the user to decide on.

## Sweep

- Type: `cpp-adoption`
- Targets: `Engine/Source/*.h`, `Engine/Source/*.cpp`, `Common/*.h`, `Common/*.cpp`
- Batches: `Engine/Source/Frame/Collections`, `Engine/Source/Frame`, `Engine/Source/Graphics/Managers`, `Engine/Source/Graphics`, `Engine/Source/Network`, `Engine/Source/Ui/Screens`, `Engine/Source/Ui`, `Engine/Source/Agent`

## Sweep rules

- Find: every hook as `## Context` defines it, declared, installed, or called in the unit. Confirm by searching the repository that the only implementation, override, derived class, or installer is in `Projects/` game code, and that the engine could name that game implementation directly.
- Target form: the engine names the expected `game::` class, global, constant,
  free function, or non-virtual member directly, as it already does for
  `game::gpGame->GetNextMusicTrack()` and `game::RegisterGameTweakSections()`.
  Delete the mechanism (pointer, `std::function`, table, registration,
  `virtual`/`= 0`, trait struct, concept, constructor parameter) and the game
  code that existed only to install it, and drop the matching `override`s. A
  game hook a second game must also provide is declared where the engine already
  expects game declarations, and a game with nothing to add gives it an empty
  body.
- Preserve behavior exactly: the call happens at the same point, in the same order, under the same `BT_CLIENT`/`BT_SERVER`, `if constexpr`, and lifetime guards. Where the indirection can actually be null at the call, the direct call keeps that guard (for example by testing the game global for `nullptr`).
- Permitted forms, never findings: everything under `## Context` "Reviewed and kept", "Not game hooks", and "Already direct"; any mechanism with an engine-side implementation or more than one implementation; and data registries that carry data rather than calls.
- Out of bound, record as deferred and leave unfixed:
  - a fix that would name a Sandbox gameplay concept (`Blaster`, `Spaceship`, `Missile`, `Omni`, `bUseMissiles`, `Fleet`) in engine code, or move fleet code into the engine;
  - a fix that needs a choice the target form does not fix: new storage, a new header, an include-order change in `Pch.h` or `Engine.h`, a renamed class or file, or a decision between several game declarations;
  - a fix that changes any byte of a saved, replayed, wire, or `.pack` format, or anything a server trust check accepts.
- Docs: propagation updates every `AGENTS.md` and `Documents/Architecture/` passage that names a removed mechanism, in the owning layer only.

## Critical files

- Every C++ file under `Engine/Source/` and `Common/` (the `## Sweep` targets), and the `Projects/BrokenEngineSandbox/Source/` files implementing a found hook, reached through propagation.
- `Documents/Investigations/ChangeWorkflow/EngineGameHookSweepDeferredFixes.md` (created by the sweep).

## In scope

- Hooks found under `## Sweep rules` in the target files, their game-side implementations and installers, project membership for any file a fix deletes, and the doc passages naming them.

## Out of scope

- Every mechanism `## Context` lists as removed, kept, not a hook, or already direct.
- Removing Sandbox gameplay names from the engine and moving game code into the engine (the game-agnostic agent commands already in `Engine/Source/Agent`, and the landed session-protocol and GameShellIntoGameBase changes).
- `Engine/Data/` shader-layout headers, `DataPacker/`, `Tools/`, and `ThirdParty/`.
- Any format, version, or protocol change.

## Acceptance criteria

1. The sweep's `CLOSE-DONE` stage report lists units swept, fixes applied, and deferred entries, and every deferred entry is in the deferred-fixes record.
2. `/compile` builds the client and server in Debug and Release (and Profile when the close line reports `profile=yes`) with no new warnings; `/update-vcxproj` validation passes when a file was deleted.
3. When the sweep changed any C++ file: an `/agent-harness` replay determinism check on a Debug build reports an identical CRC on every tick, and a live `/agent-harness` client/server session that crosses cells logs no CRC mismatch or desync. These two checks are the review this Plan states for `/sweep` `## Rules`.

## Notes

- The two hook Plans above, the session-protocol and GameShellIntoGameBase changes, and the move of the game-agnostic agent commands into `Engine/Source/Agent`, each of which changed the engine/game boundary this sweep reviews, have already landed.
- The sweep creates no follow-up Plans itself (`/sweep` `## Rules`); the user decides on the deferred entries.
