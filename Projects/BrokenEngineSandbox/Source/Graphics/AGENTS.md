# Graphics - Game Camera

Client-only `game::Camera` is the only concrete camera and is `final`. `engine::Camera` (`../../../../Engine/Source/Graphics/EngineCamera.h`) owns everything generic — the display-rate update, matrices, visible-area snapping, LOD latching, texel-grid references, zoom, jump/chase, shake, sun angle, and the `engine::gpCamera` global. This layer supplies only game policy.

## Game Policy

- `IsMainMenuFrame` and `PullTarget` cast the engine's `FrameInterpolateBase` back to `game::FrameInterpolate`, relying on the `GameBase` contract that the concrete render interpolate is the game frame type.
- `PullTarget` returns one of the three closed engine target values: the canonical menu pose (`CameraTarget::Direct`) for the main menu or an invalid client player, the raw focused-player position paired with `gpGame->mVecVisualErrorOffset` (`CameraTarget::Tracked`), or `CameraTarget::Extrapolate` after the missing-player diagnostic. The engine adds the reconciliation offset only after the raw position has updated its tracking velocity. Menu target constants stay game-owned.
- Player focus resolves through the current client coord and `PlayersPostRender`, surviving spawn and transfer. Brief lookup failures extrapolate the last focus within the engine's bounded window; an absent player identity uses the main-menu pose.
- Network reconciliation contributes a decaying camera-target offset only; authoritative simulation state is unchanged. Keep this integration consistent with `../../../../Documents/Architecture/GameReconciliation.md`.
- `SunAngle` overrides the engine's angle while the Graphics settings or ImGui UI is up. The raw angle, its reset value, and its main-menu pause stay engine-owned.
- `OnUpdateComplete` captures diff-checked client state each render frame, after matrix and visible-area calculation. Preserve the moment that state becomes visible to readers when changing zoom persistence or reset behavior.

## See Also

- `../../../../Engine/Source/Graphics/AGENTS.md` - `engine::Camera` and renderer grids
- Engine Input (`../../../../Engine/Source/Input/AGENTS.md`) - Per-frame scroll delta
