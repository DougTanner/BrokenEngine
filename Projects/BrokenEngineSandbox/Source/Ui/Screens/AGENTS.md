# Game UI Screens

The HUD and the game extension of engine TweaksScreen. The six standard player-facing menus are engine-owned (`../../../../../Engine/Source/Ui/Screens/AGENTS.md`), which also owns the shared authoring rules every screen follows — 2160-height scaling, workbuffer-backed localized text, opaque-rect registration, label-as-automation-API, font balance, panel fill, and hover animation. Visual placement and hierarchy follow `../../../../../Documents/UserInterfaceDesign.txt`; this document owns the runtime contracts of the screens that remain here.

## Shared Contracts

- Screen bodies are client-only. `HudScreen.h` is listed in the server project for IDE visibility and guards its client-only members internally, so screen headers must remain parseable in their project affinity.
- Screens gate themselves from authoritative game/UI state because `ImGuiManager` invokes main and modal surfaces independently of in-game screen gating.
- Networked controls remain disabled through `NetworkUiControl` until authoritative state resolves the request.
- The HUD is the only game UI that changes network subscription state: its fleet and player focus controls tell the client session which cells it wants, tagged with an explicit reason. A rebuilt focus button that drops that call leaves the client drawing a cell it never subscribed to. Its nav-delay slider likewise sends one fleet request on drag release, not one per frame, and marks itself pending until the server answers.
- The three HUD-specific layout fractions and the HUD's own composition stay game-local, named.
- The game TweaksScreen is a developer tool gated twice: it compiles in only under the `kbDebugInput` switch, and even then draws only while the runtime ImGui toggle is on. It is not a player-facing screen.

## Ownership

- `ImGuiManager` owns screen invocation and submission; this directory owns game-state gating and interaction semantics for the HUD.
- Engine TweaksScreen owns the section registry, persistence, tables, and slider mapping. Game Tweaks files register their own whole sections through it and implement extension hooks for sub-tabs of engine sections.
- Making the agent UI snapshot visible to readers belongs to `../../../../../Engine/Source/Agent/AGENTS.md`; visual inspection and input command semantics are outside this directory.
