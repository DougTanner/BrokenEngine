<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:37.023Z","dependsOn":[]} -->
# Reset HUD pending controls across session replacement

## Context

The retained survivor `CAI/shard-0058/001` identifies stale count-based HUD
pending bits across client reset. `NetworkUiControl<T>::Update` clears only
when an authoritative value differs and `SetPending` has no generation/reset
semantics (`Engine/Source/Ui/NetworkUiControl.h:18-47`). The HUD sets fleet and
spawn controls before sending requests at
`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:198-241,270,317-325`.
`Game::Reset` clears fleet/player state but not those controls
(`Projects/BrokenEngineSandbox/Source/Game.cpp:394-413`), while
`ChangeFrame`/connection loss and reconnect reuse the same long-lived HUD
(`Game.cpp:527-555`; `Engine/Source/Graphics/Managers/ImGuiManager.cpp:177-185`).

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0058.md:63`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1215`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed by routing. The ordinary disconnect/reconnect path can restart at
the same count, so no authoritative difference arrives to clear the old bit.

## Design

The author's recommendation is to reset the fleet-count and member-count
`NetworkUiControl` objects in the same `Game::Reset` boundary that discards
their state. Ensure the spawn control is also cancelled when no fleet is
focused, since its current `Update` is nested under the focused-fleet branch.
Keep the existing changed-count response behavior and the weapon/navigation
control resets; do not add a cross-session wire generation.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Game.cpp:394-413,527-555` — reset and
  frame-replacement lifecycle.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:198-241,264-328`
  — fleet/spawn pending controls and no-focused-fleet branch.
- `Engine/Source/Ui/NetworkUiControl.h:18-47` — value/pending/reset behavior.
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp:177-185` — HUD lifetime.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` and
  `Engine/Source/Ui/AGENTS.md` — network-control and session-reset contracts.

## In scope

- Resetting fleet-create and spawn-into-fleet pending state at every client
  session/frame reset that discards the corresponding authoritative state.
- Cancelling the spawn control when no focused fleet exists.
- Existing count-change clearing, request sends, and HUD lifetime.

## Out of scope

- Fleet request wire messages, server authorization, or fleet persistence.
- Weapon-mode and navigation-delay pending races, which are separate retained
  survivors.
- Reconstructing `ImGuiManager`/HUD or adding a session-generation protocol.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped game client UI/session
behavior with no wire, serialization, deterministic Frame, or CRC change.

Preserve these invariants:

- A pending request from a discarded session cannot disable a fresh session's
  control when counts happen to match.
- Count changes from an accepted request still clear the control normally.
- No-focused-fleet rendering cannot retain a spawn pending bit, and HUD
  lifetime remains unchanged.

## Acceptance criteria

- Starting a fleet-create or spawn request, dropping the connection before its
  response, and reconnecting with the same counts leaves the corresponding HUD
  controls enabled.
- A reset/load/frame replacement clears all three fleet/spawn pending paths,
  including the no-focused-fleet case.
- Successful count-changing requests still disable until their authoritative
  count update and then re-enable.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0058/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:1215`. No source fix or build
was performed during routing.
