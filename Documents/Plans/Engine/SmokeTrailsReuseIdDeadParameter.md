<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T19:42:12.991Z","dependsOn":[]} -->
# SmokeTrails Add: Delete the Dead `reuseId` Reuse Path

## Context

`SmokeTrailsPostRender::Add` takes a defaulted `reuseId` parameter
(`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h:74`,
`smoke_trails_t reuseId = {}`) and branches on it
(`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsUpdate.cpp:41-73`):
a valid id takes `AddIndexableElementWithId` and skips the `pfStartTimes`
write, an invalid one takes `AddVisualIndexableElement` and stamps the start
time that produces the documented new-trail length suppression
(`Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md`).

After `Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md` Branch B
lands, no caller anywhere supplies the argument. The two call sites are
`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:156` and
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/
Missiles.cpp:170`, and both pass three arguments, taking the default. The
missile site was the only argument-supplying caller, and that Plan proved the
id it passed was always the invalid `0` the client deserializer had just
overwritten, so the reuse branch never executed even before the deletion.

The reuse branch, the `AddIndexableElementWithId` call, and the
`pfStartTimes` suppression skip are therefore unreachable code with no
remaining producer. This was named in that Plan's own `## Out of scope`
(`:138-139`) on the assumption that "explosions and missile respawn use the
same entry point" and that the reuse capability was still wanted; the call-site
survey above shows neither caller uses it.

## Design

Recommended: delete the dead mechanism rather than retain an unreachable
capability, per the repository directives against speculative extension points
and unused compatibility paths. Concretely:

1. Drop the `reuseId` parameter from the declaration
   (`SmokeTrails.h:74`) and the definition (`SmokeTrailsUpdate.cpp:41`).
2. Replace the `if (reuseId.IsValid())` / `else` pair
   (`SmokeTrailsUpdate.cpp:52-63`) with the surviving `else` body alone:
   `AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender)`.
3. Make the `pfStartTimes` write unconditional by removing the
   `if (!reuseId.IsValid())` guard (`SmokeTrailsUpdate.cpp:69-72`), keeping the
   assignment itself, which is what the documented new-trail suppression
   depends on.

Both call sites already pass three arguments, so no caller changes.

The alternative — keep the parameter and record why the capability is retained
in `SmokeTrails/AGENTS.md` — is available if the implementing session's user
wants a future transfer-reuse fix (Branch A of the parent Plan) to keep its
landing pad. The author recommends deletion because that branch was already
decided against by the parent Plan's Branch B outcome, and the mechanism can be
restored from history if it is ever needed again.

Deliberately kept: `AddIndexableElementWithId`
(`Engine/Source/Frame/Collections/CollectionLifecycle.h:92-94`). Removing the
reuse branch leaves it with no caller in the tree, but it is a documented
generic lifecycle helper — `Engine/Source/Frame/Collections/AGENTS.md` names it
as the sanctioned path for transfer and reconnect re-adds that must preserve a
server-issued id — so it is part of the collection framework's public surface,
not dead code created by this change.

## Critical files

- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsUpdate.cpp`

## In scope

- The `reuseId` parameter in `SmokeTrailsPostRender::Add`'s declaration
  (`SmokeTrails.h:74`) and definition (`SmokeTrailsUpdate.cpp:41`).
- The `reuseId.IsValid()` branch and its `AddIndexableElementWithId` call
  (`SmokeTrailsUpdate.cpp:52-63`) and the `!reuseId.IsValid()` guard around the
  `pfStartTimes` write (`SmokeTrailsUpdate.cpp:69-72`), inside that one
  function.
- Any `SmokeTrails/AGENTS.md` sentence the removal makes inaccurate. A survey
  at Plan-authoring time found none — that document describes the new-trail
  suppression, which this change preserves — so an edit there is expected only
  if the implementing session finds one.

## Out of scope

- `AddIndexableElementWithId` itself and every other
  `CollectionLifecycle.h` helper, for the reason stated in `## Design`.
- The explosion caller's behavior and the missile caller's behavior; both keep
  calling `Add` exactly as they do today.
- `pfStartTimes` semantics, the smoothing constant, the W-overload invariant,
  and every other SmokeTrails behavior.
- Wire records, `TransferData`, `engine::kuiProtocolVersion`,
  `FrameInput::kiVersion`, and `Frame::kiVersion` — this change touches none of
  them.
- Restoring any form of trail identity carry across a cell transfer.

## Risk tier and invariants

Tier 2. Trigger: scoped runtime behavior of one client-only collection. The
whole file is `BT_CLIENT`-guarded (`SmokeTrailsUpdate.cpp`), smoke trail ids
come from the visual counter through `AddVisualIndexableElement`, and no
SmokeTrails column is in a `SharedMembers()` set, so nothing here reaches the
shared CRC, the wire format, or save/replay compatibility. Escalate only if
implementation finds a shared-state reader this survey missed. The one behavior
to preserve is the new-trail length suppression that the `pfStartTimes` stamp
drives, which the surviving branch already performs for every existing caller.

## Acceptance criteria

- The client builds.
- No `reuseId` reference remains in the tree.
- With `/agent-harness`, a missile in flight and an explosion each still render
  a smoke trail that grows from the spawn point rather than appearing at full
  length, matching behavior before the change.

## Notes

Source: proven out-of-scope residual from the session implementing
`Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md` Branch B, which
names the `reuseId` parameter in its own `## Out of scope` (`:138-139`).
Sequenced after that Plan through `dependsOn` because the last
argument-supplying call site disappears only when Branch B lands.
`Documents/Plans/Engine/TransferWireRecordCleanup.md` is deliberately not a
dependency: it owns dead columns of the cross-cell wire record and pays a
protocol-version bump, while this Plan touches only a client-only collection
entry point and shares no file with it.
