<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:33.003Z","dependsOn":[]} -->
# Resolve no-op HUD navigation-delay requests

## Context

The retained survivor `CAI/shard-0040/002` identifies a value-only pending
state that has no acknowledgement path. `NetworkUiControl<T>::Update` clears
pending only when the next authoritative value differs from the captured value
(`Engine/Source/Ui/NetworkUiControl.h:18-35`). The HUD marks the navigation
delay control pending on every edit release at
`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:330-349`, even
when the slider returns to its original value. The server accepts and syncs
that same value at
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:535-557`,
so the client sees an equal state forever.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0040.md:97`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1020`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed during routing. The accepted path uses valid in-range UI input and
does not depend on a lost packet.

## Design

The author's recommendation is to keep the existing value-change behavior but
avoid entering the pending state for an edit whose final value equals the
current authoritative fleet value. Also clear the control when its focused
fleet disappears or is replaced before a response, so a silently ignored
request cannot strand the HUD. Do not add a wire acknowledgement for a
presentation-only no-op unless the existing game response cannot provide the
needed lifecycle signal; changed requests continue to clear on a differing
FleetSync.

## Critical files

- `Engine/Source/Ui/NetworkUiControl.h:18-47` — pending/value semantics.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:330-350` —
  slider edit/release and pending admission.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:70-82,298-304`
  — FleetSync application and request send.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:535-557`
  — server assignment and response.
- `Engine/Source/Ui/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` — network-control
  resolution contract.

## In scope

- Navigation-delay edit-release admission and pending-state resolution.
- Equal-value responses and focused-fleet disappearance/replacement cleanup.
- Existing changed-value request, FleetSync, and slider behavior.

## Out of scope

- Fleet navigation policy, delay range/precision, packet layout, or server
  authorization.
- Generic pending-control redesign for unrelated HUD controls.
- Network transport loss and reconnect handling except for the local control
  reset needed when its target disappears.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped game UI/network
request behavior with no wire, serialization, deterministic Frame, or CRC
change.

Preserve these invariants:

- A changed navigation-delay request remains disabled until its authoritative
  FleetSync resolves it.
- An equal-value or abandoned-target request never leaves the slider pending
  indefinitely.
- Fleet GUID request identity, range, server assignment, and sync payload stay
  unchanged.

## Acceptance criteria

- Dragging a navigation-delay slider away and back to its original value sends
  no pending no-op, and the control remains immediately usable.
- A changed value stays pending until FleetSync and then becomes usable.
- If the focused fleet disappears before response, the control is reset and a
  replacement fleet's slider is not disabled by the old request.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0040/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:1020`. No source fix or build
was performed during routing.
