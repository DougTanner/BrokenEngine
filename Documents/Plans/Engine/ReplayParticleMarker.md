<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:33:31.064Z","dependsOn":[]} -->
# Preserve the replay marker through explosion particle staging

## Context

The accepted finding `CAI/shard-0018/001` identifies a reconciliation ordering
gap. `ReconcileRunTickCoord` pre-stamps `pNext->interpolate.frameFlags` with
`kRecalculated` before `RunFrameTick`
(`Engine/Source/Network/Client/ReconcileReplayTick.cpp:113-130`), but
`FrameInterpolateBase::Update` overwrites the marker from the previous frame
(`Engine/Source/Frame/FrameBase.cpp:184-199`). The normal rollback base is clear
until `ReconcileValidateCrcCoord` clears the bit after the tick
(`ReconcileReplayTick.cpp:193-224`). `ExplosionsPostRender::Spawn` therefore
publishes GPU particles during a replay (`ExplosionsSpawn.cpp:218-234`) instead
of suppressing the duplicate visual required by
`Engine/Source/Frame/Collections/Explosions/AGENTS.md`.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The marker loss
is unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to make replay state explicit in the existing
`ActiveFrameRef`: add a client-only `bRecalculated` input, set it on the
reconciliation replay ref, and leave normal dispatch refs false. `RunFrameTick`
should clear or set `rNext.interpolate.frameFlags` from that input immediately
after `FrameInterpolate::Update` and before PostRender/Destroy/Spawn. This keeps
the marker independent of stale ring-slot bytes while retaining the existing
post-validation and catch-up clears. Keep all explosion random draws outside
the client particle guard, and only audit the sibling audio gate for observing
the restored marker.

## Critical files

- `Engine/Source/Frame/FrameBase.h:68-70,273-282` — client marker and active-tick reference.
- `Engine/Source/Frame/FrameBase.cpp:184-199,284-362` — interpolation assignment and phase ordering.
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp:113-130,193-224,341-395` — replay ref and marker cleanup.
- `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:188-234` — unconditional random draws and particle gate.
- `Engine/Source/Audio/StaticVoices.cpp:189-205` — sibling marker consumer (read-only audit reference).

## In scope

- Carrying the explicit replay marker from reconciliation through
  `FrameInterpolate::Update` to the PostRender Destroy/Spawn phases.
- Clearing the marker for normal ticks and retaining existing validated-frame/
  catch-up cleanup.
- Verifying the explosion particle gate and random-draw placement against the
  restored marker; no independent audio policy change.

## Out of scope

- Explosion particle tuning, staging capacity, random algorithms, CRC fields,
  audio admission/lifecycle, or full Graphics recreation.
- Reconciliation rollback policy, ring-size changes, transfer ordering, or a
  new persisted/wire marker.
- Suppressing random draws or removing the client-only guard.

## Risk tier and invariants

Expected Change Workflow Tier 3. The change crosses client reconciliation/replay
phase ordering and controls a non-CRC side effect that must remain in lockstep
with deterministic random consumption.

Preserve these invariants:

- Replayed explosions do not publish duplicate GPU-particle bursts, while
  ordinary ticks publish exactly one.
- All explosion random draws happen on every build/tick in the same order.
- The marker is client-only, excluded from CRC/serialization, explicit for each
  tick, and cleared after validation/catch-up as today.

## Acceptance criteria

- A reconciliation replay of an explosion produces no additional particle
  staging entry, while its speculative/ordinary counterpart produces one.
- Random-engine state and shared CRCs remain unchanged by the marker fix, and
  the sibling audio replay gate observes the same marker without a separate
  audio behavior change.
- Client and server `Debug|x64` builds clean through `/compile`; an
  `/agent-harness` replay scenario exercises an explosion correction and particle
  staging count.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
