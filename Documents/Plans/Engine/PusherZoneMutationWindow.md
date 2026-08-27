<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:33:37.180Z","dependsOn":[]} -->
# Rebuild pusher zones after same-phase owner removals

## Context

The accepted finding `CAI/shard-0022/001` identifies a deterministic phase-window
failure. `FramePostRenderBase::Update` calls `SetupZones` before the game runs
`PlayersPostRender::ProcessUpdateStatusChanges`
(`Engine/Source/Frame/FrameBase.cpp:211-235`,
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:188-206`). A supported
`kDestroyPlayer` path then removes that player's pusher. Later Spaceship update
queries call `ApplyPush` with the stale thread-local zone indices
(`PushersUpdate.cpp:118-180`), so swap-and-pop can inject a ghost force, miss a
valid force, or index past the live row range. Pusher velocity reaches the
shared CRC, violating the same-phase acceleration contract in
`Engine/Source/Frame/Collections/Pushers/AGENTS.md`.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The stale-zone
window is unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to remove the `SetupZones` call from the engine
base wrapper and invoke it once in the game `FramePostRender::Update` after
`PlayersPostRender::ProcessUpdateStatusChanges` and before the game collection
Update fold. This keeps the acceleration snapshot after all same-phase player
removals and before Spaceship/other pusher consumers, while preserving the
existing thread-local ownership, ascending registration, per-zone cap, and
same-tick query window. Do not add a second stale setup call or remap indices
inside `ApplyPush`.

## Critical files

- `Engine/Source/Frame/FrameBase.cpp:211-235` — base PostRender update and current setup call.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:188-206` — player status removal and game collection order.
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:48-180` — setup/query snapshot and force application.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:246-261` and
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:649-653` — supported removal/consumer paths.
- `Engine/Source/Frame/Collections/Pushers/AGENTS.md` and `Engine/Source/Frame/AGENTS.md` — phase, thread, and determinism authority.

## In scope

- Moving the one `SetupZones` invocation to the game update point after player
  status-change pusher removals and before later pusher consumers.
- Preserving the setup/query thread-local lifetime, registration order, cap,
  and `ApplyPush` force math.
- Updating the frame-phase architecture note if the existing documented call
  location becomes inaccurate.

## Out of scope

- Signed zone-index representation/capacity (`CAI/shard-0022/002`), pusher
  overflow break logging (`PusherOverflowBreakPerCall.md`), owner pusher policy,
  force falloff, collection layout, serialization, or new pusher types.
- Adding clamps, stale-ID recovery, or a second rebuild inside `ApplyPush`.
- Any client render/profile-counter behavior.

## Risk tier and invariants

Expected Change Workflow Tier 3. The change modifies fixed simulation phase
ordering and thread-local query state feeding CRC-visible Player/Spaceship
velocity.

Preserve these invariants:

- Every `ApplyPush` query sees only live rows from the current pusher collection,
  and each zone index remains valid through all same-phase consumers.
- Player removals cannot inject or omit a same-tick deterministic impulse.
- Setup remains allocation-free/thread-local, registration and per-zone cap stay
  unchanged, and client/server phase order remains paired.

## Acceptance criteria

- A tick that destroys a player pusher before a later Spaceship query applies no
  ghost/moved-row force and performs no out-of-range pusher access.
- A valid surviving pusher still affects later same-phase queries with the same
  falloff/order, and client/server CRCs remain equal for the removal scenario.
- Client and server `Debug|x64` builds clean through `/compile`; an
  `/agent-harness` scenario exercises player removal with a nearby Spaceship.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate. The existing overflow-logging Plan shares a file but explicitly
excludes `ApplyPush`, phase timing, and collection semantics.
