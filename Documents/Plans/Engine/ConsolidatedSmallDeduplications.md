<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T13:27:15.524Z","dependsOn":[]} -->
# Consolidated small de-duplications

## Context

This Plan merges seventeen small de-duplication and redundancy-removal Plans
into one change, done all at once, by the user's direction. Each `###` item
below keeps its source Plan's findings, and the same number names it in every
later section. The items edit independent regions; where two touch the same
file, `## Coordination` says so.

Every line number is from the commit its item names (or, where it names none,
from the commit that added the source Plan); locate each site by symbol if it
moved. Nothing was built or run for any item: every finding is from a code
read, and no item's evidence records a harness setup, so the executing session
builds every scene `## Acceptance criteria` needs.

Shorthand: `BES/` is `Projects/BrokenEngineSandbox/Source/`; `FC/` is
`BES/Frame/Collections/`; `R/` is `Engine/Source/Graphics/Render/`; `S/` is
`Engine/Data/Shaders/`.

### 1. Audio harness rig snapshot assigned once

Commit `4fa6d2d7`, in
`Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`.
`CommandAudioStreamingHarnessRig` writes
`rResult = BuildAudioStreamingHarnessRigSnapshot(); return;` twelve times
(`:275-276`, `285-286`, `292-293`, `299-300`, `305-306`, `311-312`,
`317-318`, `324-325`, `358-359`, `364-365`, `370-371`, `376-377`). No action
builds a different result. The only other exits are the two `DeferResponse`
returns (`:327-335`, `:340-353`), the three throws (`:273`, `:283`, `:379`),
and the validation throws. Unmet criterion: the snapshot result is assigned
once for every non-deferred action. The value is small (about twenty lines
removed for one added nesting level and one empty branch), and the shape
departs from the early-return dispatch form the directory uses
(`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1255-1275`), which is why
the Design states the shape.

### 2. NetworkSimulation.h duplicate code

`Engine/Source/Network/NetworkSimulation.h`, verified against `10d84ca6`;
lines at `c675b6b2` except the `NetworkSimulation.h` and `EngineSwitches.h`
lines, which are after `keNetworkSimulation` moved to
`Engine/Source/EngineSwitches.h`.

- `GetNetworkSimulationBounds`: the `kDisabled` arm (`:53`) holds the same
  initializer as the `default` arm (`:59`). The sibling switches (`:14-25`,
  `:27-38`) carry `kDisabled` through `default` alone. The one caller,
  `Engine/Source/Profile/ProfileNetworkScreen.cpp:209`, sits inside
  `if constexpr (keNetworkSimulation != kDisabled)` (`:207`).
- `ProcessDelayed` and `FlushDelayed` (`:202-234` with their caller) are one
  drain loop differing only by the `releaseTime <= now` term (`:206` against
  `:216`); each is called only from `ProcessOrFlush` (`:228`, `:232`), whose
  only callers are `Engine/Source/Network/Client/Client.cpp:163` and
  `Engine/Source/Network/Server/Server.cpp:133`, inside
  `if constexpr (keNetworkSimulation != kDisabled)`. The committed value is
  `kDisabled` (`EngineSwitches.h:100`), so the default build never
  instantiates these templates.

Unmet criterion: one drain loop and one row per distinct tuning value.

### 3. RemoveIndexableElement reuses DestroyElement

`Engine/Source/Frame/Collections/CollectionLifecycle.h`, commit `ae681f03`.
`RemoveIndexableElement` (`:103-119`) repeats the whole body of
`DestroyElement` (`:186-192`): the same last-row test, two `SwapElement`
calls, and two count decrements; it adds only the `lastId` capture, the map
re-point of the moved row, and the map erase. Its first line,
`ASSERT(rInterpolate.iCount > 0)` (`:103`), restates what
`idToIndexMap.at(id)` already enforces; both callers
(`Engine/Source/Frame/Collections/PointLights/PointLights.cpp:15`,
`Engine/Source/Frame/FrameUtils.h:402`) pass `pIds[i]` of a live row, and
`.agents/references/cpp-conventions.md` forbids useless ASSERTs. Unmet
criteria: one function owns the paired swap-and-pop; no assert restates what
the next line enforces.

### 4. ListenerLoop stop recheck

`Engine/Source/Agent/AgentCommandServer.cpp`, commit `4fa6d2d7`. After
accepting a connection, `ListenerLoop` takes `mMutex`, tests
`stopToken.stop_requested()`, and on stop closes the socket and breaks
(`:163-170`). The recheck excludes nothing: stop can be requested the instant
after `:170` releases the lock, and every later point already handles it —
the `ServeConnection` loop condition (`:191`), the `ReadExact` checks (`:452`,
`:472`), the locked check at `:226`, and the wait predicate at `:242`. With
the block removed and stop already requested, `ServeConnection` returns
without touching the socket or a shared slot; the teardown block (`:178-183`)
bumps `miConnectionGeneration`, resets `mOptionalPendingResponse`, and closes
the socket; the loop top (`:132`) breaks. The socket is closed exactly once
either way. Neither the bump nor the reset is observed: the previous
teardown already reset the slot, `PublishResponse` (`:438`) refuses a stale
generation, and `request_stop`'s one caller, the destructor (`:108`), runs on
the main thread after `ClearDeferredResponse` (`:98`). Unmet criterion:
`ListenerLoop` checks stop once per iteration before handing the socket to
`ServeConnection`. Low priority: one lock acquisition per connection and
eight lines.

### 5. Collection buffer publish sequence

`Engine/Source/Frame/Collections/CollectionMemory.h`, commit `ae681f03`.
`AllocateAndAssign` (`:136-144`) and `Allocate` (`:211-219`) end with the same
sequence: set `iCapacity`, set `iPhysicalLayoutCapacity`, move the new buffer
into `pData`, then walk the member tuple with `AssignAligned`. They differ
before it (reuse test `:113`, byte ceiling `:122`, failure paths `:124-125`,
`:131-132` against `:208`). The sequence is the one
`Engine/Source/Frame/Collections/AGENTS.md` `## Core Contract` constrains
("Allocation failure must not publish a capacity or layout that was not
installed"). Unmet criterion: one function publishes capacity, physical
capacity, and buffer and walks `AssignAligned`.

### 6. Common path-to-UTF-8 helper

Commit `401e4b75`. Three file-local functions convert a
`std::filesystem::path` to a UTF-8 `std::string` with the same body (take
`u8string()`, build a `std::string` from its pointer and size), differing only
in the local's name and `c_str()` versus `data()`:

- `PathToString`, `Engine/Source/Graphics/Screenshot.cpp:20-24`, called at
  `:188`, `:437`, `:464`, and `:494`.
- `PathToUtf8`, `Engine/Source/Agent/AgentCommandsServerGeneric.cpp:19-23`,
  called at `:138`, `:140`, and `:164`.
- `PathToUtf8`, `DataPacker/Source/DiagnosticReporter.cpp:9-13`, called at
  `:65`.

`Common` has no shared owner (a search of `Common/**/*.h` for
`Utf8|WideTo|ToNarrow|u8string` finds nothing), and all three programs include
`Common.h`. Unmet criterion: one path-to-UTF-8 definition under one name.
Authority: root `AGENTS.md` `## Directives` (DRY; one term per concept);
`.agents/references/cpp-conventions.md` code placement (Common owns code more
than one program needs). Cost: the edit touches a header `Common.h`
aggregates, so every precompiled header rebuilds.

### 7. Screenshot and render-target dump helpers

Commit `401e4b75`, all in `Engine/Source/Graphics/Screenshot.cpp`. The
screenshot path (`SaveScreenshot`) and the dump path (`EncodeAndWriteDump` and
its caller) repeat three pieces of code:

1. Failure report. `ReportCaptureFailure` (`:26-35`) logs one string at
   `kWarning` and, when the request publishes a result, publishes it as the
   `error` result; seven early-return sites use it. Both `catch` blocks
   (`:194-203`, `:506-515`) re-implement it inline, logging at `kError` a
   prefixed text (`"SaveScreenshot failed: {}"`, `"DumpRenderTarget failed:
   {}"`) while publishing the bare `what()`. The rule that every failure
   publishes an error result so the deferred agent poll never hangs
   (`:386-387`) therefore has three hand-kept bodies.
2. Default output directory. The block that calls `GetTempPathW`, checks the
   length, appends `"Screenshots"`, and creates the directory is written twice
   (`:152-161`, `:406-415`), differing only in the failure text.
3. Red/blue swap. The expression swapping the red and blue bytes of a 32-bit
   pixel is written twice (`:123`, `:452`); the screenshot forces
   `0xFF000000`, the dump keeps the source alpha.

Unmet criteria: capture failures publish through one body; the default
directory and the swap expression each have one definition.

### 8. Smoke swirl time offset

Commit `c675b6b2`. The smoke swirl noise's circular time offset exists twice
with no shared constant: GLSL, `S/Smoke/SmokeSpreadCommon.h:20`,
`2 * (-1 + 2*sin(0.01 t), -1 + 2*cos(0.01 t))` with `t` =
`globalLayout.fElapsedTime`; CPU, `RenderFrameGlobal` at
`R/GlobalUniforms.cpp:613-617`, the same term times
`gSmokeCurlSpeed * kfSmokeCurlTimeScale`, published as `f2SmokeCurlTimeOffset`
(`S/ShaderGlobalLayout.h:86`) and read at `S/Smoke/SmokeSpreadCommon.h:63`.
The CPU comment at `R/GlobalUniforms.cpp:611-612` names the shader's number,
so the rate, amplitude, and form are coupled across two languages. The GLSL
term is invocation-invariant and runs per invocation in both spread passes
(`S/Smoke/SmokeSpreadOne.comp`, `S/Smoke/SmokeSpreadTwo.comp`). Authority:
root `AGENTS.md` `## Directives` (a value lives once at its owner);
`S/AGENTS.md` `## Shared Contract` supports the direction only. Unmet
criterion: the swirl offset's rate, amplitude, and form are defined once, and
both the swirl and curl offsets derive from it.

### 9. Water size quotients

Commit `c675b6b2`, except `R/WaterUniforms.cpp` lines, which are from the
commit that added the source Plan. `R/AGENTS.md` `## Precision and History`
requires each scaled size, frequency, or tile size to be the identical float
in its origin reduction and its published uniform. Two quotients meet that
only because the expression is copied:

- Normal size, three bands: published by `RenderLightingMain` at
  `R/LightingUniforms.cpp:234-236`
  (`gLightingSampledNormals*Size / gWaterScale`), reduced by
  `PopulateWaterReducedUv` at `R/WaterUniforms.cpp:58-60`.
- Color-noise frequency: reduced at `R/WaterUniforms.cpp:156`, published by
  `PopulateWaterParameters` at `:277`
  (`gWaterColorNoiseFrequency / gWaterScale`).

Three comments carry the rule (`R/LightingUniforms.cpp:232`,
`R/WaterUniforms.cpp:56`, `:277`). No current bug: every site is a
`float / float` of the same operands, on the main thread in one frame. Unmet
criterion: each scaled size or frequency has one expression both its
reduction and its publication read.

### 10. Battleship fleet-health damage step

Commit `022915b6`. `FC/Battleships/BattleshipsCombat.cpp` writes the same
damage step three times — subtract the damage from `pfHealths`; at 0 or
below, hold a fleet-controlled row at 0 and start no explosion, otherwise
call `BeginExplosion` — in `BattleshipsPostRender::ApplyBeamDamage`
(`:56-67`), `PostCollision` (`:130` and `:140-150`, with the hit sound and
flash between the subtraction and the test), and `AreaDamage` (`:187-198`).
They differ only in how each leaves its loop (`return`, `continue` or
`break`, `continue`). The rule is `FC/Battleships/AGENTS.md`
`## Fleet Health Invariants`. Unmet criterion: the fleet-health rule has one
writer.

### 11. Escort slot point scans once

Commit `fe1fb69b`. `ChooseFleetTactic` calls the file-local `EscortSlotPoint`
twice for an escorting fleet Fighter
(`FC/Ships/FleetCoordination.cpp:272-273`), with the same `rPreviousFrame`,
`rView`, and `globalId` and `iTickOffset` 0 then `kiEscortLeadTicks`. The row
scan (`:132-148`) producing `iCount` and `iSlot` reads only
`pFlagshipGlobalIds`, `pLifecycleFlags`, and `pGlobalIds`; `iTickOffset` is
first read at `:152`. The only route is a Patrol fleet Fighter's escort tick
(`FC/Fighters/Fighters.cpp:633` to `ChooseFighterFleetTactic` to
`ChooseFleetTactic` at `:301`; an Omni returns before the pair at
`:268-271`). Pre-existing debt under root `AGENTS.md` KISS and DRY. The saving
is real but small and unmeasured: fleet Fighter counts are bounded
(`kiMaximumFleetSquadronsPerType`, `kpiFleetSquadronCapacities` in `Fleet.h`),
and this change only halves the scan.

### 12. Missile steering arms

Commit `022915b6`. In `MissilesPostRender::Update`
(`FC/Missiles/MissilesUpdate.cpp`), the `if (bRetained)` arm (`:160-173`) and
its `else` arm (`:174-186`) run the same six steps: subtract the frame delta
from `deltaRotationDelay`, take the Z of the cross product of the current
direction with a wanted direction, pick a signed turn rate from its sign,
compute the delay percentage, scale the rate by it, and blend it into
`fDeltaRotation`. They differ only in the wanted direction (normalized vector
to `retained.vecPreviousPosition`, `:164-166`, against `vecStoredDirection`,
`:178-179`) and the rate constant (`kfDeltaRotationTowardsTarget`, `:167`,
against `kfDeltaRotationTowardsStored`, `:180`).

### 13. ModelViewer uses the engine Pch.cpp and engine Tweak declarations

Commit `b5c75660`. A new game project would start by copying a project's
layout, so the user asked for these two parts before that project starts.

(a) `Pch.cpp`. `Projects/ModelViewer/Source/Pch.cpp` and
`Engine/Source/Pch.cpp` are byte-identical: one line, `#include "Pch.h"`. The
ModelViewer project compiles its own copy as the precompiled-header creator
(`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` line
126, `ModelViewer.vcxproj.filters` line 13); the BrokenEngineSandbox client
and server compile the engine file for the same purpose
(`BrokenEngineSandbox.vcxproj` 551-553, `BrokenEngineSandboxServer.vcxproj`
400-402).

(b) Tweak extension declarations. Four engine files call a game function by
name: `game::RenderLightingTweakExtensions`
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` 197),
`game::RenderSmokeTweakExtensions` (`TweaksScreenSmoke.cpp` 66),
`game::RenderSoundTweakExtensions` (`TweaksScreenSound.cpp` 26) and
`game::RenderWindTweakExtensions` (`TweaksScreenWind.cpp` 57). Each includes
the game header `Ui/Screens/TweaksScreen/Tweaks.h` for the declaration. The
four declarations are written identically in two game headers:

- `Projects/ModelViewer/Source/Ui/Screens/TweaksScreen/Tweaks.h` 9-12, which
  holds nothing else (the file is 14 lines);
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h`
  23-26, among that game's own Tweaks declarations.

The ModelViewer defines the four with empty bodies
(`Projects/ModelViewer/Source/ModelViewerApplication.cpp` 1463-1478), as
`Projects/ModelViewer/AGENTS.md` `## Engine integration` records.

### 14. Navigation-delay range bounds

From the commit that added the source Plan. The admitted navigation-delay
range is written as literals in two places tied only by comments:
`IsNavigationDelayInRange` (`FC/Ships/ShipTypes.h:182-187`), whose comment
says the range matches the HUD slider, and the HUD Nav Delay slider
(`ImGui::SliderFloat("Nav Delay", ...)`, `BES/Ui/Screens/HudScreen.cpp:394`,
comment `:392-393`). Every other check calls the predicate
(`BES/Network/Server/ServerSession.cpp:36`,
`BES/Network/Server/ServerFleetSerialization.cpp:132`,
`FC/Fighters/Fighters.h:217`, `FC/Omnis/Omnis.h:240`,
`FC/Battleships/Battleships.h:210`,
`BES/Agent/Commands/ServerSimulationHarnessRigs.cpp:284` and `:417`). Drift
would give a slider zone whose release silently reverts, or an admitted range
the slider cannot reach; the release handler (`HudScreen.cpp:398-401`)
reverts a value failing the predicate before the send. False condition
(root `AGENTS.md` DRY): the admitted range has one code owner every reader
uses.

### 15. Shared Fighter and Battleship AvoidTerrain

`FightersPostRender::AvoidTerrain`
(`FC/Fighters/FightersNavigation.cpp:17-61`) and
`BattleshipsPostRender::AvoidTerrain`
(`FC/Battleships/BattleshipsNavigation.cpp:17-61`) are the same function line
for line with the type name swapped. Each file holds only that function and
two equal threshold constants (`:13-14`: enemy angle 0.4, enemy distance 40).
The per-row gate order is: dying, arrival grace, the fleet heading cone, the
enemy-proximity skip, terrain avoidance, then the turn-rate ramp. Both take
`iStart` and `iEnd`; each has one caller passing `0` and the collection's row
count (`FC/Fighters/Fighters.cpp:740`, `FC/Battleships/Battleships.cpp:557`;
declarations `FC/Fighters/Fighters.h:305`, `FC/Battleships/Battleships.h:293`).
`[[maybe_unused]]` sits on `rFrame` and `rPreviousFrame`, which both bodies
use. The pass writes `rCurrentInterpolate.pfDeltaRotations[i]`, CRC'd
simulation state; each row reads only itself and the previous frame.
`FC/Ships` already shares Fighter and Battleship code through free functions
taking `HeadingMovementConstants` (`RampHeadingTurnRate`) and file-local
templates in `FC/Ships/ShipBehaviours.cpp` (`IsNavRectangleRow`), which
already includes `FC/Ships/ShipQueries.h`. Authority tension, settled by the
Design: `FC/Battleships/AGENTS.md` and root `AGENTS.md` keep the mirror
parallel; root `AGENTS.md` also says DRY. The comment at
`FC/Fighters/Fighters.cpp:657-658` names this pass `AvoidTerrain`; the shared
function keeps that name, so the comment stays true.

### 16. Ship navigation derives caller-invariant inputs

Commit `fe1fb69b`. `ComputeNavigation` (`FC/Ships/ShipBehaviours.h:133`,
defined at `FC/Ships/ShipBehaviours.cpp:567`) takes three parameters every
ship script passes identically (`Fighters.cpp:644`, `Battleships.cpp:469`,
`Omnis.cpp:662`): `vecCellCenter`, the same expression in each script
(`Fighters.cpp:577`, `Battleships.cpp:425`, `Omnis.cpp:571`) with no other
use; `deltaTime`, built from `rFrame.interpolate.fDeltaTime`
(`Fighters.cpp:561`, `Battleships.cpp:411`, `Omnis.cpp:561`), where `rFrame`
is already a parameter; and `flagshipGlobalId`,
`rPrevious.pFlagshipGlobalIds[i]` at all three calls, while the function
reads `pGlobalIds` itself through the locator (`ShipBehaviours.cpp:569-572`).
Low priority: the verifying review rated the benefit marginal.

### 17. TickFleetTimers single wanted-cell test

Commit `4cf9d607`. `FleetNavigationController::TickFleetTimers`
(`BES/Network/Server/FleetNavigationController.cpp:62`) tests
`rFlagship.coordinate == rFleet.wantedCoordinate` at `:78` to gate the timer
drain and negated at `:89` to `continue`, under a comment (`:88`) about a
cross-cell move. Nothing between the tests changes either coordinate: when
the flagship is not at its wanted cell, nothing drains and the code leaves
through the `continue` at `:85` or `:91`, with no fleet write, fleet RNG draw,
or frame input change; when it is, the test at `:89` is always false. One
leading `continue` plus an unconditional drain and the existing `> 0.0f` test
behaves identically, including for a NaN timer.
`Documents/C++StyleGuide.txt` rule 62 asks for early returns in guard
sequences. The function feeds the fleet RNG and the fleet checksum
(`BES/Network/Server/AGENTS.md` `## Deterministic Tick Contracts`,
`BES/Save/AGENTS.md` `## Replay Contract`).

## Design

Every item is behavior-preserving except item 7's log level and error text
and item 8's CPU-versus-GPU rounding, both stated below. No item bumps a
`kiVersion` or changes a wire, save, replay, or `.pack` byte.

1. Turn the action tests of `CommandAudioStreamingHarnessRig` (`:268-379`)
   into an `if`/`else if` chain that falls through to one
   `rResult = BuildAudioStreamingHarnessRigSnapshot();` as the last statement
   of the Debug body. No helper. The chain cannot be flat:
   `RequireAudioStreamingHarnessRigFields(rParameters, {})` at `:296` must run
   after the three field-taking actions and before any no-field action's side
   effect. Outer chain: `play`, `hold_read`, `load_chunk`, each keeping its own
   field check and throw, then a final `else` holding the `:296` check and an
   inner chain over `inspect`, `clear`, `suspend`, `resume`,
   `clear_history`, `release_read`, `stall_loaders`, `release_loaders`,
   `stop_publication`, `resume_publication`, then a final `else` throwing the
   schema string as `:379` does. `inspect` has an empty body.
   `clear_history` calls `DeferResponse` and returns only when
   `BeginHistory()` returns false, otherwise falls through. `release_read`
   keeps its `DeferResponse` and `return`. `:296` and `:379` throw the same
   string, so which rejects an unknown action is not observable.
2. Delete the `kDisabled` arm at `:53` so `default` carries it, and fold
   `ProcessDelayed` and `FlushDelayed` into `ProcessOrFlush` as one loop that
   drains while the queue is non-empty and either fast-forward is on or the
   front packet's release time has passed. No helper; `ProcessOrFlush` keeps
   its signature.
3. `RemoveIndexableElement` looks up the index, reads `lastId` from the last
   row before calling `DestroyElement` (which overwrites the row), calls
   `DestroyElement`, then re-points `lastId` when the removed row was not the
   last and erases `id`. Delete the `ASSERT` at `:103`. The ID map is in the
   collection CRC (`Engine/Source/Frame/Collections/Collection.h:197-219`);
   the map contents are unchanged (when the removed row is last,
   `lastId == id` and the erase undoes any assignment; the CRC sorts keys).
4. Delete `:163-170` only. The stop-under-lock rule in
   `Engine/Source/Agent/AGENTS.md` `## Architecture` exists for the
   condition-variable waits and is not touched.
5. One helper in `CollectionMemory.h` holds the shared publish-and-assign
   sequence, takes the member tuple as a forwarding reference, and is called
   from both sites, setting the same three fields in the same order. The
   `ASSERT` at `:221` stays in `Allocate`. `iCapacity` is CRC'd
   (`Engine/Source/Frame/Collections/Collection.h:261`).
6. Add one `common::PathToUtf8` beside the other string helpers
   (`Common/StringUtils.h`, defined where that header's functions are
   defined) with the existing body; remove the three file-local definitions;
   rename the four `PathToString` call sites in `Screenshot.cpp`. Behavior is
   moved, not changed.
7. In `Screenshot.cpp`:
   - Route both `catch` blocks through `ReportCaptureFailure`. This is a
     behavior change, not a pure merge, because the log level and the text
     differ. The author recommends passing the prefixed text as the one
     string: the log line stays attributable to its function, the published
     `error` gains the same prefix, and the level becomes `kWarning` like the
     other seven capture failures, since a failed developer capture halts
     nothing. The plan review at claim time confirms the level and the text;
     the alternatives are a log-level parameter on the helper, or the bare
     `what()` for both. The two catch blocks log at `kError` today, so that
     review confirms the change of level.
   - One file-local default-directory resolver used by both paths, taking the
     failure text from its caller.
   - One file-local `static constexpr` function beside `IsBlueGreenRedAlpha`
     that swaps red and blue and keeps alpha; the dump site calls it, and the
     screenshot site calls it and then forces alpha, so both outputs stay
     byte-identical.
   - Do not merge the two encoders: alpha policy, resize, and output differ.
8. Add one `vec2` to `GlobalLayout` holding the swirl time offset, written in
   `RenderFrameGlobal` beside the curl computation; read it at
   `S/Smoke/SmokeSpreadCommon.h:20` in place of the inline expression; compute
   `f2SmokeCurlTimeOffset` from the same CPU value times
   `gSmokeCurlSpeed * kfSmokeCurlTimeScale`. Update the comment at
   `R/GlobalUniforms.cpp:611-612` so it no longer restates the shader's
   number. The swirl result changes only by CPU-versus-GPU sine and cosine
   rounding. One new field is the author's recommendation, not a user
   decision. Shader repack: the Local generation build through `/compile`,
   since `GlobalLayout` is a shared CPU/GLSL header.
9. Color noise: compute the quotient once in `PopulateWaterParameters` and
   pass it to `PopulateWaterReducedUv` (same file, no new interface). Normal
   size: one function returning the `float` quotient for a band, declared in
   `R/Render.h`, called at `R/LightingUniforms.cpp:234-236` and
   `R/WaterUniforms.cpp:58-60` (different files and entry points, so a shared
   function is the smallest shape). Each returns the same `float / float`.
10. One file-local function beside `BeginExplosion` takes the frame, the
    emitter coordinate, the row, and the damage, applies the step, and
    reports whether the row began its explosion; all three sites call it.
    `PostCollision` keeps its `BT_CLIENT`-only sound and hit flash before the
    call and still leaves the collision loop with `break` once the row is
    dying; a fleet row held at 0 keeps iterating its remaining collision
    results. Do not route `AreaDamage` through `ApplyBeamDamage` (it would
    need a dummy shooter position). The order of health writes, the
    `BeginExplosion` call, and every random draw and UUID mint is unchanged.
11. Inside `FleetCoordination.cpp`, run the row scan once per escorting
    Fighter and produce both the slot point and the lead point from the one
    `iCount` and `iSlot`. The phase arithmetic at `:152-156` runs once per
    offset exactly as today, so both results stay bit-identical. How the two
    points are returned (a small file-local result struct, or a scan helper
    returning `iCount` and `iSlot`) is the executing session's trivial
    choice. Update the comment above `EscortSlotPoint` (`:124-126`).
12. Keep one copy of the six steps in place. Before them, choose the wanted
    direction and the rate: `vecStoredDirection` and
    `kfDeltaRotationTowardsStored` by default, replaced inside
    `if (bRetained)` by the normalized vector to the retained target and
    `kfDeltaRotationTowardsTarget`. No new function. Take the default after
    the target-loss block (`:149-157`), because `:155` reassigns
    `vecStoredDirection`. Every floating-point expression keeps its operands
    and order, and the cross product keeps the current direction first, so
    the result is bit-identical under `/fp:strict`
    (`Documents/FloatingPointDeterminism.txt`).
13. (a) The ModelViewer project compiles `Engine/Source/Pch.cpp` with
    `PrecompiledHeader` Create, as the other two projects do, and
    `Projects/ModelViewer/Source/Pch.cpp` is deleted. `Pch.h` still resolves
    to the viewer's own header through the project include directories.

    (b) The four declarations move to one engine header, declared inside
    `namespace game`; `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
    is the suggested place because both game headers already include it.
    Then:

    - the four engine files stop including the game `Tweaks.h` where the
      call was their only use of it;
    - the ModelViewer `Tweaks.h` is deleted, with its project and filter
      items (`ModelViewer.vcxproj` 411, `ModelViewer.vcxproj.filters` 331)
      and its include at `ModelViewerApplication.cpp` 12;
    - the BrokenEngineSandbox `Tweaks.h` drops lines 22-26;
    - the ModelViewer's four empty definitions stay. They are the
      definitions the engine calls, not stand-ins for an unused dependency,
      so the rule against dummy stand-ins in `Projects/ModelViewer/AGENTS.md`
      `## Engine integration` is not touched, and no default body is added
      to the engine.

    One point for the executing session to check before editing:
    `Engine/Source/AGENTS.md` `## Hub Conventions` says an engine-owned
    signature must not name a game-only concept, and no engine header
    declares a `game::` function today (engine headers forward-declare
    `game::` types only, for example
    `Engine/Source/Frame/Collections/Collection.h` 8-14). These four are names
    every project must supply for the engine, not one game's concept, which
    is why the source Plan reads the rule as allowing the move. If the
    executing session or a reviewer reads it otherwise, part (b) is surfaced
    to the user and part (a) proceeds alone.

    `Engine/Source/Main.cpp` 15 includes the game `Tweaks.h` for
    `game::RegisterGameTweakSections` (line 313) and is unchanged; the
    ModelViewer has its own `Main.cpp`.
14. Declare the two bounds as named constants beside
    `IsNavigationDelayInRange` in `FC/Ships/ShipTypes.h`; the predicate tests
    against them, its comment drops the number, and the slider passes the
    same constants. `ShipTypes.h` already reaches `HudScreen.cpp` through
    `Game.h` and `Fleet.h`. The predicate keeps its name, signature, and the
    single range test that also fails NaN and both infinities. The five
    defaults equal to the maximum (`BES/Fleet.h:120`,
    `BES/Frame/StatusChange.h:107`, `FC/Fighters/Fighters.h:319`,
    `FC/Omnis/Omnis.h:376`, `FC/Battleships/Battleships.h:307`) and the
    comment at `BES/Network/Server/ServerSession.cpp:32` stay: a default is
    not the admitted range.
15. The user's recorded choices: the pass is shared as one function in
    `FC/Ships/ShipBehaviours` called by both collections (keeping two copies
    was not chosen), and the two skip thresholds become one shared pair in
    `ShipBehaviours.cpp` (a pair per type was not chosen). Steps:
    - Add one function named `AvoidTerrain` to `FC/Ships/ShipBehaviours.h` and
      `.cpp`, keyed by `ShipType` and that type's `HeadingMovementConstants`,
      with no range and no threshold parameters; its body is a file-local
      template over the Interpolate and PostRender collection types, as
      `IsNavRectangleRow` is, instantiated for Fighters and Battleships. The
      four threshold constants become one pair beside it with the same
      values. Move the expressions verbatim in the same order;
      `ShipTypeClearanceTier` takes the passed type.
    - `FightersPostRender::Update` and `BattleshipsPostRender::Update` call it
      at the same point (after the row loop) with no row range; it loops from
      0 to the `iCount` of the collection it reads through `rFrame`, the same
      row set.
    - Delete both `AvoidTerrain` declarations and both `*Navigation.cpp`
      files, and remove the files from the client and server projects and
      filters through `/update-vcxproj`.
    - Invariants: gate order and every expression unchanged; each row reads
      only itself and the previous frame; the pass runs once per type per
      tick after that type's row loop and before Transfer, Destroy, and
      Spawn; the comments giving those reasons move with the code.
16. Remove `vecCellCenter`, `deltaTime`, and `flagshipGlobalId` from
    `ComputeNavigation` and derive them inside: the cell centre from the same
    expression the scripts use, with the W=1 rationale comment the scripts
    carry on that local (`Omnis.cpp:568-570` and counterparts) moved with
    it; the tick delta from `rFrame.interpolate.fDeltaTime`; the flagship ID
    from the row's `pFlagshipGlobalIds` in `rPreviousFrame` through the
    locator. Caveat: the existing `pGlobalIds` read beside it is from the
    current `rFrame`; `pFlagshipGlobalIds` is equal in both frames only
    because Spawn alone writes it, so the previous-frame read is required as
    exactly the value passed today. If that read cannot be made exactly
    equal, `flagshipGlobalId` stays a parameter and only the other two go.
    The three ship scripts change together, keeping Fighters and Battleships
    the parallel pair `FC/Battleships/AGENTS.md` describes.
17. In the per-fleet loop of `TickFleetTimers`, after the dead-flagship
    `continue`: add one `continue` when the flagship is not at
    `rFleet.wantedCoordinate`; make the `cellChangeTimerSeconds` drain
    unconditional; keep the `> 0.0f` `continue`; delete the second coordinate
    test with its `continue` and comment; keep the content of the comment
    above the drain (drain only while at the wanted cell, even in cross-cell
    mode; one tick per pass), placed with the new guard and the drain.

## Critical files

- `Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp` (1)
- `Engine/Source/Network/NetworkSimulation.h` (2)
- `Engine/Source/Frame/Collections/CollectionLifecycle.h` (3)
- `Engine/Source/Agent/AgentCommandServer.cpp` (4)
- `Engine/Source/Frame/Collections/CollectionMemory.h` (5)
- `Common/StringUtils.h` and its implementation file,
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`,
  `DataPacker/Source/DiagnosticReporter.cpp` (6)
- `Engine/Source/Graphics/Screenshot.cpp` (6, 7)
- `Engine/Data/Shaders/ShaderGlobalLayout.h`,
  `Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h`,
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp` (8)
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`,
  `Engine/Source/Graphics/Render/LightingUniforms.cpp`,
  `Engine/Source/Graphics/Render/Render.h` (9)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
  (10)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`
  (11)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`
  (12)
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` and
  `ModelViewer.vcxproj.filters`; `Projects/ModelViewer/Source/Pch.cpp`
  (deleted); `Projects/ModelViewer/Source/Ui/Screens/TweaksScreen/Tweaks.h`
  (deleted); `Projects/ModelViewer/Source/ModelViewerApplication.cpp`;
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h`;
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`;
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`,
  `TweaksScreenSmoke.cpp`, `TweaksScreenSound.cpp`, `TweaksScreenWind.cpp`;
  `Projects/ModelViewer/AGENTS.md` (13)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h`,
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp` (14)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
  and `ShipBehaviours.cpp` (15, 16)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`,
  `Fighters/FightersNavigation.cpp` (deleted),
  `Battleships/Battleships.h`, `Battleships/BattleshipsNavigation.cpp`
  (deleted), and `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/`
  `BrokenEngineSandbox.vcxproj` and `BrokenEngineSandboxServer.vcxproj` with
  their `.filters` (15)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
  and `Battleships/Battleships.cpp` (15, 16), `Omnis/Omnis.cpp` (16)
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`
  (17)

## In scope

1. The action tests of `CommandAudioStreamingHarnessRig` (`:268-379`) and its
   twelve snapshot tails.
2. `GetNetworkSimulationBounds`'s `kDisabled` arm; `ProcessDelayed`,
   `FlushDelayed`, and `ProcessOrFlush`.
3. The body of `RemoveIndexableElement`.
4. The locked `stop_requested` block between the non-blocking socket
   configuration and the `ServeConnection` call in
   `AgentCommandServer::ListenerLoop`.
5. The publish-and-assign tail of `AllocateAndAssign` and of `Allocate`, and
   the one new helper.
6. One new `common::PathToUtf8` declaration and definition; the file-local
   `PathToString` in `Screenshot.cpp` and its four call sites; the
   file-local `PathToUtf8` in `AgentCommandsServerGeneric.cpp` and in
   `DiagnosticReporter.cpp`, and their call sites as far as the qualified
   name requires.
7. In `Screenshot.cpp`: the two `catch` blocks (`:194-203`, `:506-515`); the
   two default-directory blocks (`:152-161`, `:406-415`) and one new
   file-local resolver; the two swap expressions (`:123`, `:452`) and one new
   file-local swap function.
8. One new `vec2` field in `GlobalLayout`; the smoke curl time offset block of
   `RenderFrameGlobal` (`R/GlobalUniforms.cpp:611-617`); the `f2TimeNoise`
   expression at `S/Smoke/SmokeSpreadCommon.h:20`.
9. `PopulateWaterReducedUv`: the three `fSizeBase*` initializers, the
   `fNoiseFrequency` initializer, and its parameter list;
   `PopulateWaterParameters`: the `fWaterColorNoiseFrequency` write and the
   `PopulateWaterReducedUv` call; `RenderLightingMain`: the three
   `fLightingSampledNormals*Size` writes; one function declaration in
   `R/Render.h`; the three rule comments named in item 9's Context, where the
   shared expression makes them redundant.
10. A new file-local damage function next to `BeginExplosion`, and the damage
    steps in `BattleshipsPostRender::ApplyBeamDamage`, `PostCollision`, and
    `AreaDamage`.
11. The file-local static `EscortSlotPoint` and its comment
    (`FleetCoordination.cpp:124-157`), and its two calls in
    `ChooseFleetTactic` (`:272-273`).
12. `MissilesPostRender::Update`: the `if (bRetained)` / `else` steering arms.
13. The `Pch.cpp` compile item and filter item of the ModelViewer project, and
    the deletion of the viewer's `Pch.cpp`. The four `Render*TweakExtensions`
    declarations: added once in the engine header, removed from both game
    `Tweaks.h` files. The `Tweaks.h` include line in the four engine files and
    in `ModelViewerApplication.cpp`, and the viewer `Tweaks.h` project and
    filter items. `Projects/ModelViewer/AGENTS.md` `## Engine integration` and
    the engine Tweaks documentation, where they say who declares the four
    functions.
14. `IsNavigationDelayInRange`, its comment, and two new bound constants beside
    it in `FC/Ships/ShipTypes.h`; the two bound arguments of the Nav Delay
    `ImGui::SliderFloat` call in `HudScreen.cpp`.
15. `FightersPostRender::AvoidTerrain` and
    `BattleshipsPostRender::AvoidTerrain`: declarations, definitions, their
    `iStart` and `iEnd` parameters, their four threshold constants, and their
    one call each with its two range arguments; one new function, its
    file-local template, and the shared threshold pair in
    `FC/Ships/ShipBehaviours.h`/`.cpp`; deleting `FightersNavigation.cpp` and
    `BattleshipsNavigation.cpp` and their project and filter entries.
16. `ComputeNavigation`: declaration, definition parameter list, and the
    derived locals at the top of its body; in each ship script's PostRender
    `Update`, the `vecCellCenter` local and its comment and the arguments of
    the one `ComputeNavigation` call.
17. `FleetNavigationController::TickFleetTimers`: the lines from the comment
    above the drain through the second coordinate test.

Documentation: any `AGENTS.md` sentence `/update-claude-docs` finds this
change makes false, including where a sentence names item 15's moved
function or its file, and `FC/Ships/AGENTS.md` `## Ship Invariants` and
`BES/Ui/Screens/AGENTS.md` `## Shared Contracts` for item 14.

## Out of scope

- 1: error text, action names, required fields, and the order of side
  effects; the two deferred poll lambdas beyond staying in their branches;
  the sentinel literal at `:347`; every other dispatch chain under
  `Engine/Source/Agent`.
- 2: every tuning row's value and the other two switches; the drop
  simulation and its log, `DelayedPacket`, and `NetworkSimulationState`; the
  committed value and location of `keNetworkSimulation`; both callers of
  `ProcessOrFlush`.
- 3: `DestroyElement`, `SwapElement`, the signature of
  `RemoveIndexableElement`, and both callers.
- 4: `ServeConnection`, `ReadExact`, `Drain`, `PublishResponse`, and the
  destructor; the connection teardown block and the generation counter;
  whether a request from a closed connection still runs.
- 5: `GrowCapacityWithCopy` (`:320-341`), which copies before replacing
  `pData`; the reuse test, byte ceiling, and failure paths of both functions.
- 6: any change to the produced string; other path or string conversions,
  including `PathToCppVariable`; the rest of `Screenshot.cpp` beyond item 7.
  A further copy in `Projects/Coretide` (not searched) is reported as a
  residual, not edited.
- 7: `ReportCaptureFailure`'s seven existing call sites; the default-file
  counters (`siScreenshot`, `siDump`); flattening the screenshot's row/column
  loop and renaming its pixel buffers; capture-result fields other than the
  `error` text; merging the two encoders.
- 8: the wind-noise `sin`/`cos` pair at `S/Smoke/SmokeSpreadCommon.h:16`; any
  other smoke tunable, the curl scale, and `kfSmokeCurlTimeScale`'s value.
- 9: the `LerpAtHeight` fades (`R/LightingUniforms.cpp:242-244`,
  `R/WaterUniforms.cpp:62`); moving any water-normal Main field out of
  `RenderLightingMain` (ModelViewer calls `RenderWaterMain` only when the
  world is visible,
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp:1487-1491`); the
  foam tile sizes and wave parameters; the wrapper minimums and
  `PopulatePatchLattice`.
- 10: Fighters' damage sites (`FC/Fighters/FightersCombat.cpp:215`, `:276`,
  `:310`); Omnis' already-shared step; `BeginExplosion`, the
  `ApplyBeamDamage` signature, the cell-exit transfer test in
  `PostCollision`, and the `AreaDamage` query; `AGENTS.md` rule text.
- 11: `FleetCoordination.h` and every caller outside the file; the escort
  slot rule (slot order, lap clock, lead, speed formula); the scan's growth
  with rows times escorting Fighters, such as a per-fleet cache on
  `FleetBoard`; `FindSquadronTarget` and every other function in the file;
  `FC/Fighters/AGENTS.md`.
- 12: the target-loss block, the jitter block, and every other statement of
  `MissilesPostRender::Update`; the steering constants; what `Transfer` sends
  as the stored direction; any `kiVersion`.
- 13: the four functions' signatures, call sites, and bodies, in the engine,
  the ModelViewer, and BrokenEngineSandbox; `Pch.h` in any project and the
  BrokenEngineSandbox project files; the ModelViewer `Main.cpp` and a shared
  bootstrap for it; any other ModelViewer file or setting.
- 14: the range's values and the predicate's name, signature, and callers;
  the slider's release handling, function-static edit state, and the
  navigation request; the five default initialisers and the
  `ServerSession.cpp` comment; `pNavigationDelays`,
  `SpawnInfo::navigationDelay`, and everything else
  `Documents/Plans/Game/RemoveShipNavigationDelayColumn.md` owns.
- 15: any behavior change (gate order, threshold values, cone test, idle-row
  behavior); `OmnisNavigation.cpp` and Omni steering; `ComputeTerrainAvoidance`,
  `FindNearestEnemyShip`, `HasLineOfSight`, and `RampHeadingTurnRate`;
  sharing any other mirrored Fighter and Battleship code.
- 16: the island candidate list each script passes to
  `ComputeReturnDestination`
  (`Documents/Plans/Game/ReturnDestinationReadsCentredIslands.md`);
  the navigation modes, random draws, arrival checks, and path-query cadence;
  the other `ComputeNavigation` parameters; the in-function repeats inside
  `ComputeNavigation` (`Documents/Plans/Game/ReduceShipBehavioursCpp.md`);
  the `rVecArtificialIntelligenceDirection` parameter name
  (`Documents/Plans/Game/RenameArtificialIntelligenceToAi.md`); `pGlobalIds`
  as a simulation input outside the CRC
  (`Documents/Plans/Game/IncludeShipGlobalIdsInCrc.md`); the mode 6 and 7
  random-draw rule
  (`Documents/Investigations/Engine/ResolveModeSixSevenRandomDrawRule.md`).
- 17: the rest of `TickFleetTimers` (pick logic, member loop, log),
  `FlagshipReachesEdge`, and `ReachableNeighbourDirections`; any change to
  when a timer drains, when a fleet picks, or how many fleet RNG draws a
  tick makes; turning `FleetNavigationController` into a free function.

## Risk tier

Tier 3 — invariant/integration, the highest of the items, per
`.agents/references/risk-tiers.md`. Triggers:

- Threading (item 4): a locked stop check on the listener thread's shutdown
  path is removed, resting on an interleaving argument made by code reading.
- Data layout (item 8): the shared CPU/GLSL `GlobalLayout` gains a field;
  smoke is client-only visual state outside the CRC, and the server includes
  the header and is rebuilt.
- A change spanning independently owned subsystems (item 6, and the merged
  change as a whole): `Common`, engine Graphics, Agent, Network, Frame, and
  Render, DataPacker, the ModelViewer, and BrokenEngineSandbox simulation,
  server, and UI; the `Common.h`-aggregated header edit rebuilds every
  precompiled header.

Lower-tier exposure the review still covers:

- Determinism/CRC paths whose computed values must not change (items 3, 5,
  10, 11, 12, 15, 16, 17; item 14's predicate is shared by server admission,
  grid-save validation, ship `PostRead`, and the harness rig, and must admit
  exactly the same values). Item 17 must leave the fleet RNG and fleet
  checksum bit-identical.
- Public header names removed or added (items 2, 9, 15, 16), declarations
  moved between headers (item 13), and project membership (items 13, 15).
- Developer-tool behavior: item 7 changes the log level and text of the two
  catch paths and the published `error` text; item 1 is `BT_CLIENT`,
  Debug-only.

No wire, serialization, save, replay-format, `.pack`, or `kiVersion` change.

## Acceptance criteria

Shared verification runs once for the whole change. Before editing, the
executing session captures every "before" artifact below on the session
baseline build.

Code reads:

1. `CommandAudioStreamingHarnessRig` assigns
   `BuildAudioStreamingHarnessRigSnapshot()` to `rResult` once; the `:296`
   check still runs after the three field-taking actions and before every
   no-field action's side effect.
2. `GetNetworkSimulationBounds` has no `kDisabled` arm, and one loop drains
   the delayed packets.
3. `RemoveIndexableElement` contains no `SwapElement` call, no count
   decrement of its own, and no `ASSERT` on `iCount`.
4. `ListenerLoop` holds one stop check per iteration, at the loop top, and
   passes the accepted socket straight to `ServeConnection`.
5. The publish-and-assign sequence appears once in `CollectionMemory.h`.
6. The tracked tree holds one path-to-UTF-8 definition, `common::PathToUtf8`,
   and no file-local `PathToString` or `PathToUtf8`.
7. Neither `catch` block builds an `error` result itself; both call
   `ReportCaptureFailure`; `GetTempPathW` and the swap expression each
   appear once in `Screenshot.cpp`.
8. The literal rate 0.01 and the circular form appear once, in
   `RenderFrameGlobal`; `SmokeSpreadCommon.h` reads the published field.
9. Each of the four water quotients is written as a division in exactly one
   place.
10. The subtract, clamp, and `BeginExplosion` step appears once in
    `BattleshipsCombat.cpp`.
11. One escort tick walks the previous-frame Fighter rows once, a single scan
    feeding both points, with the per-offset phase arithmetic unchanged.
12. One copy of the six steering steps remains in `MissilesPostRender::Update`.
13. The ModelViewer project compiles `Engine/Source/Pch.cpp` as its
    precompiled-header creator, and nothing references the deleted
    `Projects/ModelViewer/Source/Pch.cpp`. A search finds each of the four
    `Render*TweakExtensions` declarations exactly once, in the engine header,
    and finds neither deleted file nor a reference to it.
14. The two navigation-delay bounds appear as literals once each, at the named
    constants, read by both `IsNavigationDelayInRange` and the Nav Delay
    slider; the predicate still rejects NaN and both infinities.
15. The gate order exists in one `AvoidTerrain` function taking no row range
    and no thresholds; neither call site passes a row range; the thresholds
    exist once; neither collection declares `AvoidTerrain`; both
    `*Navigation.cpp` files are gone from the tree, both projects, and their
    filters.
16. `ComputeNavigation` has no `vecCellCenter` or `deltaTime` parameter, and
    no `flagshipGlobalId` parameter unless item 16's fallback applied, which
    the executing session reports with its reason; any in-function flagship
    read is from `rPreviousFrame`.
17. `TickFleetTimers` compares the flagship's coordinate with
    `wantedCoordinate` once per fleet, before the drain.

Builds and static checks (one set):

18. One Local generation build through `/compile` exports every shader with
    no error and builds the BrokenEngineSandbox client in Debug and Release,
    the server, the ModelViewer, and DataPacker (every project that compiles
    `Common`).
19. The repository static checks pass, including `/update-vcxproj`
    validation of the ModelViewer and both BrokenEngineSandbox projects and
    their filters.
20. `/glsl-review` reports no CPU/GLSL layout finding for item 8.
21. Item 2 only: with a temporary, uncommitted non-disabled
    `keNetworkSimulation`, the client and server build, and an
    `/agent-harness` session at that level stays connected with fast-forward
    toggled on and off. The landed tree keeps `kDisabled`, which criterion 18
    builds.

Harness (`/agent-harness`), on the criterion 18 build:

22. One replay determinism check, the acceptance in
    `.agents/skills/agent-harness/references/replay.md`
    `### Replay determinism acceptance`, in its stronger form: a recording
    made on the baseline build plays back on the changed build (no version is
    bumped, so it stays readable) with no frame CRC mismatch and no fleet
    checksum mismatch. That one recording must contain every scene the
    CRC-path items need: id-indexed rows destroyed (3); collection
    allocation (5, any run); fleet and random-spawn Battleships taking
    blaster, missile, and beam damage, with a fleet Battleship's health
    holding at 0 (10); fleet Fighters escorting their flagship (11); missiles
    flying with and without a target (12); fleet and random-spawn Fighters
    and Battleships flying near island terrain (15); a fleet ship of each of
    the three types reaching its `ComputeNavigation` call, including a
    Patrol fleet Fighter (`ShipOrders::kSeekAndDestroy` clear) on a tick its
    flagship is not in its cell (16); and at least one fleet flagship picking
    a new cell (17). A scene that cannot share the recording gets its own
    baseline recording under the same check.
23. A Debug client sends `audio_streaming_harness_rig` once per action, once
    with an unknown action, and once with `inspect` plus an extra field; each
    returns the same result shape or error text as on the baseline (1).
24. With default settings, the agent presentation continuity probe reports
    the same `noiseFrequency` as on the baseline (9).
25. One before/after capture set: a PNG and a JPG screenshot and a
    render-target dump of a four-channel 8-bit target, each with and without
    an explicit output path, are byte-identical to the baseline's (6, 7); and
    screenshots of one scene with smoke swirl and curl enabled look the same
    as the baseline's (8). Take the byte-identical captures in a view without
    smoke, since item 8 allows CPU-versus-GPU rounding in the swirl.
26. The harness session launches the client and the server, sends commands
    over several connections, and sends `quit` to each; both processes exit
    without a hang (4).

## Coordination

Within this Plan: items 6 and 7 both edit `Screenshot.cpp` (item 6's
`PathToString` call sites sit in the encoders item 7 edits); items 15 and 16
both edit `FC/Ships/ShipBehaviours.h`/`.cpp` and the `Update` bodies of
`Fighters.cpp` and `Battleships.cpp`, on different lines.

Other Plans that edit the same regions, with no ordering required unless
stated; whichever lands second re-derives its sites by symbol from the landed
tree:

- `Documents/Plans/Engine/NameAudioHarnessRigNoPoolIndex.md` names the
  sentinel at `:347`, inside the `release_read` branch item 1 moves.
- `Documents/Plans/Engine/RemoveCoordUpdateTickReader.md` edits
  `NetworkSimulation.h` and needs the same temporary non-disabled build as
  item 2.
- `Documents/Plans/Engine/PublishAreaToUvInverseExtents.md` changes other
  `GlobalLayout` fields (item 8).
- `Documents/Plans/Game/SkipMissileRegistryWindowWithoutMissiles.md` leaves
  item 12's steering arms to this Plan.
- `Documents/Plans/Game/RemoveShipNavigationDelayColumn.md` keeps
  `IsNavigationDelayInRange` by name, removes its three `PostRead` callers
  and the spawn-payload check, and deletes `SpawnInfo::navigationDelay`
  (item 14); it also edits `Fighters.cpp`, `Battleships.cpp`, `Fighters.h`,
  and `Battleships.h` (item 15). Other Plans cite `HudScreen.cpp` by line
  number, which item 14 can shift: `Documents/Plans/Game/`
  `FleetOwnedSquadronState.md`, `IncludeShipGlobalIdsInCrc.md`,
  `RemoveClientStateFromReplayMeta.md`, and
  `RemoveShipNavigationDelayColumn.md`.
- Item 15's files are also edited by `Documents/Plans/Game/`
  `ReturnDestinationReadsCentredIslands.md`, `IncludeShipGlobalIdsInCrc.md`,
  and `SyncFighterBeamHalfWidthParameter.md`.
- `Documents/Plans/Game/ReduceShipBehavioursCpp.md` depends on this Plan
  (item 16) and measures `ShipBehaviours.cpp` with item 15's function in it.
- `Documents/Plans/Game/RenameArtificialIntelligenceToAi.md` renames a
  parameter on the `ComputeNavigation` definition line item 16 edits.
- `Documents/Plans/Game/ReturnDestinationReadsCentredIslands.md` removes the
  island candidate block a few lines from item 16's `vecCellCenter` local and
  edits `ComputeReturnDestination` in the same two `FC/Ships` files.
- `Documents/Plans/Game/FleetOwnedSquadronState.md` edits the member loop of
  `TickFleetTimers`, not item 17's lines.
