<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T22:46:49.989Z","dependsOn":[]} -->
# Put the corresponding header first in split .cpp files

## Context

`Documents/C++StyleGuide.txt` rule 47 a) puts a `.cpp`'s corresponding `.h` first, alone in its group. `.agents/scripts/Test-IncludeOrder.ps1` now identifies that header as the `.h` in the `.cpp`'s directory whose name stem is the longest ordinal case-insensitive prefix of the `.cpp`'s name stem (script header comment, lines 9-10), so a `.cpp` that implements part of a class declared in a differently named header — `ExplosionsUpdate.cpp` for `Explosions.h`, `ServerSend.cpp` for `Server.h` — gets group 1 for that header.

Before that change, the script classified such a header as an ordinary engine or game include (group 2 or 3), whose folder-first sorting accepted it after other includes. The newly exposed violations therefore predate the script change; they were reported, not fixed, because that change was limited to the script.

Observed: `Test-IncludeOrder.ps1 -All` reports 52 violations (`status: fail`, 548 files scanned), all in these 26 files, as `<line><kind>`:

- `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp`: 1 order, 2 blank-missing
- `Engine/Source/Frame/Collections/Explosions/ExplosionsUpdate.cpp`: 1 order, 2 blank-missing
- `Engine/Source/Frame/IslandTerrainResidency.cpp`: 3 order, 5 blank-missing
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`: 3 order, 4 and 5 blank-missing
- `Engine/Source/Network/Client/ClientReceive.cpp`: 3 order, 4 blank-missing
- `Engine/Source/Network/Server/ServerReceive.cpp`: 6 blank-missing
- `Engine/Source/Network/Server/ServerSend.cpp`: 5 order, 6 and 7 blank-missing
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp`: 1 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`: 4 order, 9 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp`: 3 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`: 1 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp`: 1 order, 10 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`: 1 order, 9 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp`: 3 order, 10 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsCombat.cpp`: 1 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp`: 1 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp`: 3 order, 6 blank-missing
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp`: 1 order, 4 and 5 blank-missing
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionSubscriptions.cpp`: 2 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp`: 1 order
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenLightingEffectsLighting.cpp`: 1 order, 4 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenLightingEffectsVisible.cpp`: 1 order, 4 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenParticles.cpp`: 1 order, 4 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenSmokeDeposits.cpp`: 1 order, 4 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenSoundEffects.cpp`: 1 order, 4 blank-missing
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenWindDeposits.cpp`: 1 order, 4 blank-missing

Examples: `TweaksScreenHexShield.cpp` lines 1-4 include `Ui/Screens/TweaksScreen/TweaksSliderMap.h` and `Ui/HexShieldWrappersBase.h` before `TweaksScreen.h`; `IslandTerrainResidency.cpp` lines 3-5 include `Graphics/Managers/TextureManager.h` and `Graphics/Islands.h` before `IslandTerrain.h` in the same group. A trial `-All -Fix` in a disposable copy, followed by `-All`, left 0 violations, and sampled rewrites were true positives.

## Design

The recommended approach is to let the script's own fixer perform the reorder, because every reported kind (`order`, `blank-missing`) is fixable and the fixer changes only the reordered include segments. Run the invocation `.agents/skills/code-style-review/references/worker.md` documents for rule 47 once over the 26 paths above:

`pwsh -NoProfile -Command "& '.agents/scripts/Test-IncludeOrder.ps1' -RepositoryRoot '<absolute repository toplevel>' -Path '<file>','<file>' -Fix"`

Then rerun `-All` without `-Fix` to confirm no violation remains anywhere, and inspect each rewritten file's diff, which should touch only its include block. Build every owning target through `/compile`; the build is the meaning-preservation proof for the reorder. A build failure caused by a corresponding header that is not self-contained is a blocker to report, not something to fix inside this Plan.

If a listed file was already reordered or removed by the time this runs (for example by a style-guide sweep stage), the `-All` rerun is still the acceptance check; skip such paths rather than recreating them.

Change Workflow tier: Tier 1 — trigger: style-only, behavior-preserving include reordering with no public signature or invariant exposure.

## Critical files

- The 26 `.cpp` files listed in `## Context`.
- `.agents/scripts/Test-IncludeOrder.ps1` (run only).

## In scope

- The `#include` blocks of the 26 listed files: moving the corresponding header first in its own group and the blank-line changes the fixer makes.

## Out of scope

- Any other line of those files, and any other C++ file.
- Merging split `.cpp` files back into one file per class, renaming files, and project or filter membership.
- `.agents/scripts/Test-IncludeOrder.ps1`, `Documents/C++StyleGuide.txt` rule 47, and `/code-style-review` text.
- Adding includes to a corresponding header that turns out not to be self-contained.

## Acceptance criteria

- `Test-IncludeOrder.ps1 -All` exits 0 with `status: pass` and zero violations.
- Each changed file's diff is confined to its include block.
- The client and server targets that own the changed files build cleanly through `/compile`.

## Coordination

No dependencies or mandatory coordination constraints. `Documents/Plans/Engine/StyleGuideSweepEngine.md` and `Documents/Plans/Game/StyleGuideSweepProjects.md` sweep every file under `Engine/` and `Projects/` and may touch the same include blocks; the `-All` acceptance check holds whichever lands first.

## Notes

- No determinism/CRC, serialization, wire, replay, threading, or shader exposure. No live verification is needed; the build is the proof.
- Several listed files are client-only or server-only by project membership or `#if defined(BT_CLIENT)` / `#if defined(BT_SERVER)` guards (for example `ServerSend.cpp`, `ClientReceive.cpp`, `AgentCommandsClient.cpp`), so both the client and the server builds are needed.
