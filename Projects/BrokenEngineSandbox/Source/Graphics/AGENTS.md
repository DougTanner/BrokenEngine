# Graphics - Game Camera

Client-only `game::Camera` is the only concrete camera and is `final`. `engine::CameraBase` (`../../../../Engine/Source/Graphics/CameraBase.h`) owns everything generic — the display-rate update, matrices, visible-area snapping, LOD latching, texel-grid references, zoom, jump/chase, shake, sun angle, and the `engine::gpCamera` global. This layer supplies only game policy.

## Game Policy

- `PullTarget` casts the engine's `FrameInterpolateBase` back to `game::FrameInterpolate`, relying on the `GameBase` contract that the concrete render interpolate is the game frame type.
- `PullTarget` returns one of the three closed engine target values: the canonical menu pose (`CameraTarget::Direct`) for the main menu or an invalid client owned entity, the raw focused owned-entity position paired with the inherited visual-error offset (`CameraTarget::Tracked`), or `CameraTarget::Extrapolate` after the missing-owned-entity diagnostic. Menu target constants stay game-owned.
- Owned-entity focus resolves through the cell the supplied interpolate frame is local to — not the client cell — and that cell's `OmnisPostRender`, surviving spawn and transfer. Brief lookup failures extrapolate the last focus within the engine's bounded window; an absent owned-entity identity uses the main-menu pose.
- Network reconciliation contributes a decaying camera-target offset only; authoritative simulation state is unchanged. Keep this integration consistent with `../../../../Documents/Architecture/GameReconciliation.md`.
- `SunAngle` returns the sun-angle override slider's value in place of the engine's angle while the Graphics settings or ImGui UI is up. The raw angle, its reset value, and its main-menu pause stay engine-owned.

## See Also

- `../../../../Engine/Source/Graphics/AGENTS.md` - `engine::CameraBase` and renderer grids
- Engine Input (`../../../../Engine/Source/Input/AGENTS.md`) - Per-frame scroll delta
