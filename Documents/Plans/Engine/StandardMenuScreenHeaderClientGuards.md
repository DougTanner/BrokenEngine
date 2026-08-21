<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T17:37:24.342Z","dependsOn":[]} -->
# Add whole-file BT_CLIENT guards to the engine standard menu screen headers

## Context
The six engine standard menu screen headers carry no whole-file client guard:

- `Engine/Source/Ui/Screens/MainMenuScreen.h`
- `Engine/Source/Ui/Screens/PauseMenuScreen.h`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.h`
- `Engine/Source/Ui/Screens/SoundMenuScreen.h`
- `Engine/Source/Ui/Screens/GameSettingsScreen.h`
- `Engine/Source/Ui/Screens/ModalScreen.h`

Each begins with `#pragma once` and goes straight into `namespace engine`
(`GameSettingsScreen.h` first includes `Ui/LocalizationBase.h`), while the house
pattern for a client-only engine screen header is `#pragma once`, then
`#if defined(BT_CLIENT)`, then the content, closed by
`#endif // BT_CLIENT` — shown by the sibling
`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h`. Every paired `.cpp`
already wraps its whole body in `#if defined(BT_CLIENT)` immediately after
including its own header.

Nothing misbehaves today: all six headers are client-project-only members, and
their sole consumer `Engine/Source/Graphics/Managers/ImGuiManager.h` is itself
whole-file client-guarded, so no server translation unit ever sees them. This is
convention alignment only. It was surfaced by the `/update-vcxproj` validation
run of the standard-menu-screens-to-engine change: the guard heuristic in
`Resolve-VcxprojMembership.ps1` classifies these headers as "shared" because the
guard text is absent, which is a false signal for future membership checks.

The gap pre-dates that change — the same headers were unguarded at its session
baseline, when they lived under `Projects/BrokenEngineSandbox/Source/Ui/Screens/`
— so it is a pre-existing condition the move carried across, not something that
change introduced.

## Design
In each of the six headers, insert `#if defined(BT_CLIENT)` immediately after
`#pragma once` — before any `#include`, which matters for `GameSettingsScreen.h`
— and close the file with `#endif // BT_CLIENT` after the closing
`} // namespace engine`, exactly matching `TweaksScreenBase.h`. No declaration,
include, class body, ordering, or signature changes.

## Critical files
- `Engine/Source/Ui/Screens/MainMenuScreen.h`
- `Engine/Source/Ui/Screens/PauseMenuScreen.h`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.h`
- `Engine/Source/Ui/Screens/SoundMenuScreen.h`
- `Engine/Source/Ui/Screens/GameSettingsScreen.h`
- `Engine/Source/Ui/Screens/ModalScreen.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h` — reference pattern
  only; not edited.

## In scope
- Add the opening `#if defined(BT_CLIENT)` line after `#pragma once` and the
  closing `#endif // BT_CLIENT` line at end of file in each of the six headers
  listed above.

## Out of scope
- The paired `.cpp` files, which are already whole-file guarded.
- `TweaksScreenBase.h` and any other `Engine/Source/Ui/Screens/` file.
- Any declaration, include, class member, namespace, or signature change in the
  six headers.
- Changing project or filter membership for these files, and changing
  `Resolve-VcxprojMembership.ps1` or its guard heuristic.
- Guarding unguarded headers elsewhere in the repository.

## Risk tier and invariants
Expected Change Workflow Tier 1: preprocessor-only, behavior-preserving
convention alignment on client-project-only headers with no public signature or
invariant exposure. The one invariant to keep is that the client build still
sees every declaration — verified by compiling the client. The server build is
unaffected because it never includes these headers.

## Acceptance criteria
- Each of the six headers has `#if defined(BT_CLIENT)` as the first
  non-`#pragma once` line and `#endif // BT_CLIENT` as its last line, matching
  `TweaksScreenBase.h`.
- The client and server projects both compile.
- `/update-vcxproj` validation no longer reports these six headers as "shared".

## Notes
Recorded as a pre-existing out-of-scope residual of the
standard-menu-screens-to-engine change.
