<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:29.774Z","dependsOn":[]} -->
# Blasters ClientInitialize calls SynchronizeBlaster

## Context

Line numbers are from commit `629ced2e`, except those in
`Blasters/BlastersUpdate.cpp`, which follow the removal of that file's
`Frame/TerrainUtils.h` include after that commit; locate by symbol if they
moved. Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read.

Three defects in the client-only Blasters light sync, all inside
`#if defined(BT_CLIENT)` except the one local load named last:

- Repeated logic. `BlastersInterpolate::ClientInitialize` ends with a block
  (`Blasters/Blasters.cpp:57-80`) that repeats the body of the file-static
  `SynchronizeBlaster` (`Blasters/BlastersUpdate.cpp:92-120`): the point-light
  test, `fSize`, the `PointLightsInterpolate::Sync` fields, `fWidth`,
  `fLength`, the `CalculateArea` arguments, and the
  `AreaLightsInterpolate::Sync` fields are the same. The real differences are
  the direction source (the stored `pVecDirections[iIndex]` at
  `Blasters.cpp:72` against `XMVector3Normalize(vecVelocity)` at
  `BlastersUpdate.cpp:111`) and where `rType` is looked up.
- Repeated normalize. `SynchronizeBlaster` takes `vecVelocity` and normalizes
  it at `:111`. Its only caller (`:146`) computed the same
  `XMVector3Normalize(vecVelocity)` at `:140` and stored it at `:143`.
- Guard scope. `BlastersInterpolate::Update` declares
  `[[maybe_unused]] int64_t iTypeIndex` at `:133` and loads it in both
  builds, but its only reads are at `:146` and `:150`, inside the `BT_CLIENT`
  block at `:145-159`. The load is not part of the load-and-store rule in
  `Collections/AGENTS.md` `## Game-Specific Rules`: the function never stores
  the column, and the comment at `:132` names it as copied by
  `AllocateAndCopy`. Root `AGENTS.md` `## Client/Server Targets` asks for the
  narrowest practical guard.

Originating gap: the architecture phase (cohesion and generation-residue
lens) of a findings-only deep analysis of that directory (recursive) found
the repeated block; its in-function analysis phase found the repeated
normalize and the guard scope. Each was confirmed by an independent
verification read. Pre-existing debt, not an unmet acceptance criterion of
any change. Unmet criterion: one copy of the Blasters light sync, computing
the direction once, with client-only locals under `BT_CLIENT`.

## Design

Recommended (the author's recommendation), in this order:

1. Change `SynchronizeBlaster`'s `vecVelocity` parameter to the direction,
   delete the normalize at `BlastersUpdate.cpp:111`, and pass `vecDirection`
   at `:146`.
2. Replace `Blasters.cpp:57-80` with a `SynchronizeBlaster` call that passes
   the row's lights, position, direction, and type index. With step 1 done,
   `ClientInitialize` passes the direction it reads today, so it does not
   rely on the stored direction equalling the normalized velocity for a row
   that arrived by read or transfer.
3. Move the `iTypeIndex` declaration to the first line of the `BT_CLIENT`
   block in `BlastersInterpolate::Update` and drop `[[maybe_unused]]`.

How `ClientInitialize` reaches `SynchronizeBlaster` is the one choice this
Plan pre-stages; the executing session confirms it in its plan:

- Option A, recommended: give `SynchronizeBlaster` external linkage and
  declare it by hand in `Blasters/Blasters.cpp` under `BT_CLIENT`, mirroring
  Missiles, where `SynchronizeMissile` is defined in `Missiles/Missiles.cpp`
  and declared by hand in `Missiles/MissilesUpdate.cpp:34`. Rationale: root
  `AGENTS.md` `## Directives` keeps mirrored patterns parallel, and no header
  changes.
- Option B: declare it in `Blasters/Blasters.h` under `BT_CLIENT`. It avoids
  a hand declaration but adds a client-only free function to a header in the
  game precompiled-header closure.

Risk tier: Tier 2 (the author's classification). Trigger: the edit rewrites
statements inside `BlastersInterpolate::Update`, which both builds compile
and which stores CRC'd columns, and Option B adds a header declaration. The
server-build change is the removal of one local load nothing reads, so no
stored value changes. The lights are client-only visual state outside the
CRC, and each keeps the same inputs. No wire, serialization, `kiVersion`,
replay, or threading exposure.

## Critical files

- `Blasters/Blasters.cpp`
- `Blasters/BlastersUpdate.cpp`
- `Blasters/Blasters.h` (Option B only)

## In scope

- `SynchronizeBlaster`: its linkage, its direction parameter, and the
  normalize at `BlastersUpdate.cpp:111`.
- The `SynchronizeBlaster` call and the `iTypeIndex` declaration in
  `BlastersInterpolate::Update`.
- The light-sync block at the end of `BlastersInterpolate::ClientInitialize`
  (`Blasters.cpp:57-80`).
- The one declaration the chosen option adds.

## Out of scope

- The light and wind-trail creation in `ClientInitialize`
  (`Blasters.cpp:31-55`) and the wind-trail sync in Update.
- The `pVecDirections` column and its stores, which the Plan named under
  `## Coordination` owns.
- `SynchronizeMissile` and the Missiles files.

## Acceptance criteria

1. `Blasters.cpp` holds no `PointLightsInterpolate::Sync` or
   `AreaLightsInterpolate::Sync` call, `SynchronizeBlaster` holds no
   `XMVector3Normalize`, and `iTypeIndex` is declared inside `BT_CLIENT`
   without `[[maybe_unused]]`; the diff settles this.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a client screenshot shows blaster lights
   aligned with their flight direction, both for bolts spawned while the
   client watches and for bolts present when it joins. The supplied evidence
   records no harness setup, so the executing session builds the scene.

## Coordination

`Documents/Plans/Game/RemoveBlastersDirectionColumn.md` depends on this Plan
and changes the same statements afterwards: the direction `ClientInitialize`
reads (`Blasters/Blasters.cpp:72`) and the normalize and store in Interpolate
Update (`Blasters/BlastersUpdate.cpp:140-146`). This Plan leaves
`ClientInitialize` passing the stored column to `SynchronizeBlaster` and
Update computing `vecDirection` once, outside `BT_CLIENT`, for the store at
`:143` and the call. That Plan then changes the `ClientInitialize` argument
to the normalized velocity, deletes the store, and moves the computation
into the `BT_CLIENT` block.
