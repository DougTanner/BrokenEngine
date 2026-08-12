<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:06.000Z","dependsOn":[]} -->
# Move the hex shield wrappers into the engine and drop the forced PCH include

## Context

This is a layering inversion, not just misplaced code. `Projects/BrokenEngineSandbox/Source/Ui/HexShieldWrappers.h` (27 lines) and `HexShieldWrappers.cpp` (25 lines) declare and define eleven `engine::Wrapper` globals — four edge, five wave, two direction — that tune the engine's HexShields collection rendering. The engine reads all eleven directly:

- `Engine/Source/Graphics/Render/MainUniforms.cpp:426-438` — `PopulateHexShield` reads `game::gHexShieldGrow`, `game::gHexShieldEdgeDistance`, `game::gHexShieldEdgePower`, `game::gHexShieldEdgeMultiplier`, the five wave wrappers, `game::gHexShieldDirectionFalloffPower`, and `game::gHexShieldDirectionMultiplier` into `shaders::MainLayout`.

To make that compile, the game PCH force-includes the header purely on the engine's behalf: `Projects/BrokenEngineSandbox/Source/Pch.h:101-103` wraps `#include "Ui/HexShieldWrappers.h"` in a `BT_CLIENT` guard ahead of `Engine.h`. So the engine's own uniform population depends on a game header being injected before the engine aggregation header — a second game would have to reproduce that arrangement byte for byte.

## Design

Move both files to `Engine/Source/Ui/HexShieldWrappersBase.{h,cpp}` in namespace `engine`, matching the neighbouring `*WrappersBase` pairs under `Engine/Source/Ui/`. The whole-file `#if defined(BT_CLIENT)` guard and all eleven wrapper definitions (values, ranges, and grouping comments) move unchanged; the implementation's self-include renames to the `*Base` header. The header is not added to `Engine.h`: wrapper headers are deliberately consumed directly, never aggregated (`Engine/Source/Ui/AGENTS.md`).

Then:

- `MainUniforms.cpp` requalifies the eleven reads from `game::` to `engine::` and adds `#include "Ui/HexShieldWrappersBase.h"` beside its existing `Ui/WaterWrappersBase.h` include.
- The three PCH lines at `Pch.h:101-103` are deleted; nothing else needed that forced include.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp` stays where it is — it defines `game::TweaksScreen::RenderHexShieldSection` and registers a game Tweaks section. It requalifies the eleven wrapper addresses in its `TweaksSliderMapRegistrar` and adds its own `#include "Ui/HexShieldWrappersBase.h"`, which the deleted PCH force-include used to supply.

## Critical files

- `Engine/Source/Ui/HexShieldWrappersBase.h`, `HexShieldWrappersBase.cpp` — new home
- `Projects/BrokenEngineSandbox/Source/Ui/HexShieldWrappers.h`, `.cpp` — deleted
- `Engine/Source/Graphics/Render/MainUniforms.cpp` — requalification and include
- `Projects/BrokenEngineSandbox/Source/Pch.h` — delete the forced include
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp` — requalification and include
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.filters` — the files are whole-file `BT_CLIENT` and belong to the client project only; entries repath to `Engine\Source\Ui\` and refilter from `Game\Ui` to `Engine\Ui`; the server project stays untouched (there is no separate engine project; the game vcxprojs compile `Engine/Source/`)

## In scope

- Moving both files verbatim into `engine` under the `*Base` name, with unchanged wrapper defaults and ranges
- Requalifying `MainUniforms.cpp`'s eleven reads and adding the direct include
- Deleting the `BT_CLIENT`-guarded `HexShieldWrappers.h` include block at `Pch.h:101-103`
- Requalifying `TweaksScreenHexShield.cpp`, adding its direct include, and updating client project/filter membership (`/update-vcxproj`)
- Any `Engine/Source/Ui/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`, or `Engine/Source/AGENTS.md` sentence that names the old owner or the forced include

## Out of scope

- The wind-deposit wrappers (`Ui/WindDepositsWrappers.*`, force-included unguarded at `Pch.h:104`). They have the same inversion shape — `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:120` reads `game::gWindDepositExplosionsIntensity` and `game::gWindDepositExplosionsWidth` — but those values are per-game-entity content, so who should own them is an open decision. Record it as a residual; do not move them here.
- Any change to a hex shield wrapper's default value or allowed range
- Any change to the hex shield shader or to `shaders::MainLayout`
- The PCH's documented include-order contract for `Common.h`, `ShaderLayouts.h`, and `Engine.h` (comment at `Pch.h:97-98`)
- Moving the graphics quality wrappers — owned by `Documents/Plans/Ui/GraphicsQualityWrappersToEngine.md`

## Risk tier and invariants

Tier 2 — client rendering tuning only, outside the CRC and the wire. Invariants: all eleven values and ranges stay byte-identical, so hex shield rendering is unchanged; the PCH include-order comment at `Pch.h:97-98` still describes the file after the deletion; the Tweaks slider registration order continues to match wrapper declaration order per `Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md`.

## Acceptance criteria

- Client compiles and links with no `game::gHexShield*` wrapper referenced from `Engine/Source/` (a repo grep finds the eleven wrapper reads only as `engine::`), and with no hex shield include in `Pch.h`. `MainUniforms.cpp`'s other `game::` uses (`gpGame`, `gpCamera`) are pre-existing and out of scope here.
- A screenshot of a hex shield under fire is visually identical to one captured before the change.
- The Tweaks hex shield section still lists the same sliders in the same order and still drives the effect live.

## Scores

Effort 1 / Impact 3 / Risk 1
