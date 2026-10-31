# Game UI Screens

The HUD, the map screen, and the game extension of engine TweaksScreen. The six standard
player-facing menus are engine-owned
(`../../../../../Engine/Source/Ui/Screens/AGENTS.md`), which also owns the
shared authoring rules every screen follows — 2160-height scaling,
workbuffer-backed localized text, opaque-rect registration,
label-as-automation-API, font balance, panel fill, hover animation, and
buttons a click never moves. Visual
placement and hierarchy follow
`../../../../../Documents/UserInterfaceDesign.txt`; this document owns the
runtime contracts of the screens that remain here.

## Shared Contracts

- Screen bodies are client-only. `HudScreen.h` is listed in the server project for IDE visibility and guards its client-only members internally, so screen headers must remain parseable in their project affinity.
- Screens gate themselves from authoritative game/UI state (`../../../../../Engine/Source/Ui/Screens/AGENTS.md` `## Standard Menu Contract`).
- Networked controls remain disabled through `NetworkUiControl` until authoritative state resolves the request.
- While the server plays a replay, the HUD disables every control that sends a simulation-affecting request; fleet and member focus change only subscriptions and stay enabled. The server refuses those requests during playback regardless, so this gating keeps players from sending requests that do nothing.
- The HUD is the only game UI that changes network subscription state: its fleet
  and owned-entity focus controls tell the client session which cells it wants, tagged
  with an explicit reason. A rebuilt focus button that drops that call leaves
  the client drawing a cell it never subscribed to. Its nav-delay slider
  likewise sends one fleet request on a release that changes the value, not one
  per frame, and marks itself pending until the server answers. A release whose
  typed value falls outside the range the server admits reverts to the fleet's
  current delay instead of sending, because the server treats such a delay as
  corrupt data.
- The three HUD-specific layout fractions and the HUD's own composition stay game-local, named.
- The map screen is a full-screen overlay: an opaque background and the
  world's cell hexagons, projected at ground level through the live camera,
  both at the map opacity. The map is open exactly while the camera's framing
  override is set and not releasing, so no separate open flag exists; TAB
  eases the override out to the map framing, or releases it back to the world
  framing, which the map never changes, so cells and islands load for the
  gameplay view. Opacity is a log-scale function of the rendered framing
  alone, so the fade matches the camera through a reversal mid-ease; at full
  opacity the frame is a world-hidden frame
  (`../../../../../Engine/Source/Graphics/AGENTS.md`
  `## Frame and Resource Lifecycle`). A session reset closes an open map with
  no ease. The map framing, the fully-opaque framing, and the ease
  length are the game Tweaks Map sliders, unpersisted and at their defaults
  outside `kbDebugInput` builds; their ranges keep the opaque framing above the
  ceiling and at or below every map framing, so the map always reaches full
  opacity. A map framing or ease-length change applies at the next toggle.
- Game Tweaks content is a developer tool, not a player-facing screen: the engine Tweaks screen compiles its body in only under the `kbDebugInput` switch, and the engine `ImGuiManager` owns its runtime draw gate (`../../../../../Engine/Source/Graphics/Managers/AGENTS.md` `### ImGuiManager`).

## Ownership

- Screen construction, invocation, and submission are engine-owned (`../../../../../Engine/Source/Ui/Screens/AGENTS.md`); this directory owns game-state gating and interaction semantics for the HUD.
- Engine TweaksScreen owns the section registry, persistence, tables, and slider mapping. Game Tweaks files register their own whole sections through it and draw their content inside engine sections from the game functions those sections call by name.
- Making the agent UI snapshot visible to readers belongs to `../../../../../Engine/Source/Agent/AGENTS.md`; visual inspection and input command semantics are outside this directory.
