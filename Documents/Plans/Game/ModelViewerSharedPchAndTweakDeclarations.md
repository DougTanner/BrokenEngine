<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:35:12.659Z","dependsOn":[]} -->
# ModelViewer: Use the Engine Pch.cpp and Engine Tweak Declarations

## Context

The ModelViewer carries two small files that only repeat what another file
already holds. A new game project (Coretide) would start by copying a
project's layout, so the user asked for these to be planned before that
project starts.

Nothing was built or run for this Plan: every statement below is from a code
read at commit `b5c75660`, plus one byte comparison of the two `Pch.cpp`
files. Line numbers are from that tree; locate by symbol if they moved.

(a) `Pch.cpp`. `Projects/ModelViewer/Source/Pch.cpp` and
`Engine/Source/Pch.cpp` are byte-identical: one line, `#include "Pch.h"`.
The ModelViewer project compiles its own copy as the precompiled-header
creator
(`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` line
126, `ModelViewer.vcxproj.filters` line 13). The BrokenEngineSandbox client
and server compile the engine file for the same purpose
(`BrokenEngineSandbox.vcxproj` 551-553, `BrokenEngineSandboxServer.vcxproj`
400-402) and have no `Pch.cpp` of their own.

(b) Tweak extension declarations. Four engine files call a game function by
name: `game::RenderLightingTweakExtensions`
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` 197),
`game::RenderSmokeTweakExtensions` (`TweaksScreenSmoke.cpp` 66),
`game::RenderSoundTweakExtensions` (`TweaksScreenSound.cpp` 26) and
`game::RenderWindTweakExtensions` (`TweaksScreenWind.cpp` 57). Each includes
the game header `Ui/Screens/TweaksScreen/Tweaks.h` for the declaration. The
four declarations are written identically in two game headers:

- `Projects/ModelViewer/Source/Ui/Screens/TweaksScreen/Tweaks.h` 9-12, which
  holds nothing else (the file is 14 lines);
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h`
  23-26, among that game's own Tweaks declarations.

The ModelViewer defines the four with empty bodies
(`Projects/ModelViewer/Source/ModelViewerApplication.cpp` 1463-1478), as
`Projects/ModelViewer/AGENTS.md` `## Engine integration` records.

Originating gap: a read-only audit of files a second game would have to copy.
It is not an unmet acceptance criterion of any change.

## Design

Recommended shape (the author's recommendation; rationale given):

(a) The ModelViewer project compiles `Engine/Source/Pch.cpp` with
`PrecompiledHeader` Create, as the other two projects do, and
`Projects/ModelViewer/Source/Pch.cpp` is deleted. `Pch.h` still resolves to
the viewer's own header through the project include directories.

(b) The four declarations move to one engine header, declared inside
`namespace game`; `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h` is
the suggested place because both game headers already include it. Then:

- the four engine files stop including the game `Tweaks.h` where the call was
  their only use of it;
- the ModelViewer `Tweaks.h` is deleted, with its project and filter items
  (`ModelViewer.vcxproj` 411, `ModelViewer.vcxproj.filters` 331) and its
  include at `ModelViewerApplication.cpp` 12;
- the BrokenEngineSandbox `Tweaks.h` drops lines 22-26;
- the ModelViewer's four empty definitions stay. They are the definitions the
  engine calls, not stand-ins for an unused dependency, so the rule against
  dummy stand-ins in `Projects/ModelViewer/AGENTS.md` `## Engine integration`
  is not touched, and no default body is added to the engine.

One point for the executing session to check before editing:
`Engine/Source/AGENTS.md` `## Hub Conventions` says an engine-owned signature
must not name a game-only concept, and no engine header declares a `game::`
function today (engine headers forward-declare `game::` types only, for
example `Engine/Source/Frame/Collections/Collection.h` 8-14). These four are
names every project must supply for the engine, not one game's concept, which
is why this Plan reads the rule as allowing the move. If the executing
session or a reviewer reads it otherwise, part (b) is surfaced to the user
and part (a) proceeds alone.

`Engine/Source/Main.cpp` 15 includes the game `Tweaks.h` for
`game::RegisterGameTweakSections` (line 313) and is unchanged; the
ModelViewer has its own `Main.cpp`.

Risk tier: Tier 1 (the author's classification). Trigger: project membership
plus behavior-preserving movement of declarations; no signature changes and
no generated code changes. Exposure: build only — client-only files in both
projects; no CRC, save, `.pack`, wire, replay, threading or shader exposure.

## Critical files

- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` and
  `ModelViewer.vcxproj.filters`
- `Projects/ModelViewer/Source/Pch.cpp` (deleted)
- `Projects/ModelViewer/Source/Ui/Screens/TweaksScreen/Tweaks.h` (deleted)
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`,
  `TweaksScreenSmoke.cpp`, `TweaksScreenSound.cpp`, `TweaksScreenWind.cpp`
- `Projects/ModelViewer/AGENTS.md`

## In scope

- The `Pch.cpp` compile item and filter item of the ModelViewer project, and
  the deletion of the viewer's `Pch.cpp`.
- The four `Render*TweakExtensions` declarations: added once in the engine
  header, removed from both game `Tweaks.h` files.
- The `Tweaks.h` include line in the four engine files and in
  `ModelViewerApplication.cpp`, and the viewer `Tweaks.h` project and filter
  items.
- `Projects/ModelViewer/AGENTS.md` `## Engine integration` and the engine
  Tweaks documentation, where they say who declares the four functions.

## Out of scope

- The four functions' signatures, call sites and bodies, in the engine, the
  ModelViewer and BrokenEngineSandbox.
- `Pch.h` in any project, and the BrokenEngineSandbox project files.
- The ModelViewer `Main.cpp` and a shared bootstrap for it; the user has not
  decided that.
- Any other ModelViewer file or setting.

## Acceptance criteria

1. `/compile` builds the ModelViewer and the BrokenEngineSandbox client and
   server.
2. `/update-vcxproj` validation, as far as it covers the ModelViewer project
   and its filters, passes.
3. A search finds each of the four declarations exactly once, in the engine
   header, and finds neither deleted file nor a reference to it.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
