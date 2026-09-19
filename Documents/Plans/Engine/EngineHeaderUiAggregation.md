<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-18T22:33:54.137Z","dependsOn":[]} -->
# Engine.h aggregates engine UI headers the Ui contract excludes

## Context

`Engine/Source/Ui/AGENTS.md:48` states the contract: "Engine UI headers stay out
of `Engine.h` and are included directly, or through the game facade for
localization, so tuning edits recompile little."

`Engine/Source/Engine.h:32-34` contradicts that contract by aggregating three
engine UI headers inside its `BT_CLIENT` block:

- `Ui/Screens/TweaksScreen/TweaksScreenBase.h`
- `Ui/Screens/TweaksScreen/TweaksSliderMap.h`
- `Ui/NetworkUiControl.h`

Every client translation unit therefore recompiles when any of the three
changes, which is exactly the cost the contract exists to avoid.

The aggregation also hides a second defect. `TweaksSliderMap.h` declares
`std::unordered_map<std::string_view, Wrapper*>& Get()`
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h:11`) and
`TweaksSliderMapRegistrar(std::initializer_list<std::pair<const std::string_view,
Wrapper*>>)` (`TweaksSliderMap.h:17`) while including nothing. It compiles only
because `Projects/BrokenEngineSandbox/Source/Pch.h:99` includes
`Frame/Frame.h` before `Engine.h:101`, and the Frame collection headers carry
`class Wrapper;` forward declarations
(`Engine/Source/Frame/Collections/CollectionController.h:7`,
`Engine/Source/Frame/Collections/PointLights/PointLights.h:11`,
`Engine/Source/Frame/Collections/Explosions/Explosions.h:16`). The header is not
self-contained: included first in any translation unit it does not compile.

Found as a pre-existing residual during the `get_wrapper` client agent command
change (`Documents/Plans/Engine/TweaksSliderAgentValueReadback.md`), which was
Agent-subsystem-only and left both citations untouched. That change's own
`#include "Ui/WrapperBase.h"` in
`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` is correct and stays.

## Design

The contradiction is between a documented rule and the current include graph.
Authority order puts `Engine/Source/Ui/AGENTS.md` above current code behavior,
so the recommended resolution is to make the code match the rule rather than to
relax the rule.

Recommended direction, in order:

1. Make `TweaksSliderMap.h` self-contained by adding a local `class Wrapper;`
   forward declaration inside `namespace engine` — the declarations use only
   `Wrapper*`, so no definition is needed and
   `Engine/Source/Ui/WrapperBase.h:14` need not be pulled in. The author
   recommends the forward declaration over the full include for compile cost;
   a reviewer preferring the include may substitute it without changing the
   rest of this plan.
2. Delete the three `#include` lines at `Engine/Source/Engine.h:32-34`, leaving
   the `BT_CLIENT` block's remaining contents intact (remove the now-empty
   `// Ui (client-only)` comment with them if nothing else remains under it).
3. Add the direct includes each consumer then needs. The engine Tweaks screen
   translation units already include `TweaksScreenBase.h` and
   `TweaksSliderMap.h` directly and need nothing. The game-side consumers do
   not: `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
   derives from `engine::TweaksScreenBase`, the game Tweaks screen `.cpp` files
   use `TweaksSliderMapRegistrar`, and
   `Projects/BrokenEngineSandbox/Source/Game.h:127-128` and
   `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.h:22-24` hold
   `engine::NetworkUiControl<>` members. Add the exact header each file uses,
   at its own site; the compile errors from step 2 enumerate the full set.

No behavior changes: the three headers keep their contents, their `BT_CLIENT`
guards, and every declaration they expose. This is include-graph shape only.

Change Workflow tier: **Tier 2**. Highest trigger from
`.agents/references/risk-tiers.md`: build coordination within one subsystem —
changing what `Engine.h` aggregates changes what every client translation unit
sees, so consumers across the engine and the game project must gain direct
includes in the same change. No determinism/CRC, wire, serialization, replay,
threading, or trust-boundary surface is touched.

## Critical files

- `Engine/Source/Engine.h:32-34` — the three aggregated engine UI includes to
  delete.
- `Engine/Source/Ui/AGENTS.md:48` — the rule the current graph contradicts; it
  is already correct and is not edited.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h:11,17` — `Wrapper*`
  uses with no include and no local forward declaration.
- `Projects/BrokenEngineSandbox/Source/Pch.h:99,101` — the `Frame/Frame.h`
  before `Engine.h` ordering the current build accidentally depends on.
- `Projects/BrokenEngineSandbox/Source/Game.h`,
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.h`,
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/*` — game
  consumers that reach the three headers only through `Engine.h` today.

## In scope

- Deleting the three `#include` lines at `Engine/Source/Engine.h:32-34` and the
  `// Ui (client-only)` comment if it is left with nothing to label.
- Adding a `class Wrapper;` forward declaration inside `namespace engine` in
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h`.
- Adding direct `#include` lines for `TweaksScreenBase.h`, `TweaksSliderMap.h`,
  and `NetworkUiControl.h` to the engine and game files that use those symbols
  and currently get them through `Engine.h`.

## Out of scope

- Any change to `Engine/Source/Ui/AGENTS.md:48`; the rule is the authority.
- Any change to the three headers' contents beyond the one forward declaration,
  including their `BT_CLIENT` guards and every declaration they expose.
- Any change to `Engine/Source/Ui/WrapperBase.h`, `Wrapper`, or wrapper
  declaration sites.
- Any change to `Engine/Source/Agent/`, including the `get_wrapper` handler's
  own `#include "Ui/WrapperBase.h"`.
- Reordering or otherwise restructuring `Projects/BrokenEngineSandbox/Source/Pch.h`.
- Auditing other `Engine.h` aggregation entries outside `Ui/`.

## Acceptance criteria

- `Engine/Source/Engine.h` contains no `#include "Ui/...` line.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h` compiles when it is
  the first project header a translation unit includes.
- Client and server both build clean, confirming every consumer gained the
  direct include it needs.

## Notes

- The diff is decisive for the include-graph change itself; the build is the
  evidence that the consumer set is complete.
- `Documents/Plans/Engine/TweaksSliderAgentValueReadback.md` names
  `Engine/Source/Engine.h:33` as the path by which `TweaksSliderMap.h` reaches
  the agent command unit, but lists `TweaksSliderMap.h` out of scope and does
  not own this contradiction, so it is not a duplicate of this plan.
