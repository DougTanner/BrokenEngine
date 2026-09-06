<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:38.593Z","dependsOn":[]} -->
# Cancel weapon-mode requests when their player disappears

## Context

The retained survivor `CAI/shard-0058/002` identifies a pending weapon-mode
control that is not paired with its requested player. The HUD captures only the
Boolean mode through `NetworkUiControl<bool>::Update`, sets pending, and sends
the selected global ID at
`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:389-410`.
`ServerBroadcaster::ProcessUpdatePlayerRequests` silently drops a request when
the player is no longer owned (`Engine/Source/Network/Server/ServerBroadcaster.cpp:232-285`),
and `ServerSession::BeforeNetworkPoll` drains the queue at
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:256-263`.
Automatic `FleetSelection` fallback changes selection without the manual reset
at `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp:77-87,204-240`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0058.md:81`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1224`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. A player death between render-time
input and the next server poll is an ordinary supported combat race.

## Design

The author's recommendation is to cancel the weapon-mode control whenever its
requested player identity leaves the current client authority or automatic
selection moves to another member. Pair the pending state with the requested
global player ID (or perform the equivalent comparison at the existing
selection/HUD boundary), and reset it when the focused player disappears or
changes. Keep the accepted-target path and existing manual-selection reset;
do not require a new rejection packet for a target that is already gone.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:389-412` —
  selected player read, pending control, and request send.
- `Engine/Source/Ui/NetworkUiControl.h:18-47` — pending/value state.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:232-292` and
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:256-263`
  — dropped-target request and queue drain.
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp:77-87,204-240` —
  manual and automatic selection transitions.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:91-123`
  — player disappearance events.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` — network-control
  and identity pairing contract.

## In scope

- Weapon-mode pending identity and cancellation when a target player dies,
  disappears, or automatic fallback selects another member.
- Selection/HUD reset paths needed to avoid inheriting an old player's pending
  state.
- Existing accepted-player mode update and manual selection behavior.

## Out of scope

- Server authorization, update-player wire layout, or new rejection/ack packets.
- Fleet death policy, respawn behavior, and unrelated fleet/spawn controls.
- Weapon-mode semantics for a player that remains owned and selected.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped game client UI and
selection behavior; the existing request wire and deterministic player state
remain unchanged.

Preserve these invariants:

- A pending weapon-mode request remains associated with its requested global
  player identity.
- A target disappearance or selection change clears stale pending state before
  another player's control is rendered.
- An accepted request for a still-owned target continues to clear on the
  authoritative mode update, and manual selection keeps its reset behavior.

## Acceptance criteria

- If a selected player dies before server consumption, the weapon control is
  re-enabled after the disappearance/fallback rather than staying pending.
- Automatic fallback to a replacement player cannot inherit the old player's
  pending bit, even when both players share the same mode value.
- A still-owned target's accepted mode update and manual player selection retain
  their existing behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0058/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:1224`. No source fix or build
was performed during routing.
