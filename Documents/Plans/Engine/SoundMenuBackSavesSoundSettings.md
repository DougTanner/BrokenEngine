<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:01.423Z","dependsOn":[]} -->
# Save Sound Settings When the Sound Screen's Back Is Pressed

## Context

The three settings screens differ on Back. `GameSettingsScreen.cpp:103-107`
calls `SaveGameSettings()` and `GraphicsMenuScreen.cpp:217-221` calls
`SaveGraphicsSettings()` before returning to the pause menu. The Sound screen's
Back (`SoundMenuScreen.cpp:68-71`) only sets `meUiState`.

`SaveSoundSettings` has two callers: `ResetSoundSettings`
(`SoundSettings.cpp:99`) and the shutdown block at `Engine/Source/Main.cpp:531`.
So a changed volume or `Mute in background` value reaches disk only at a clean
quit. It is lost when the process leaves the main loop by an exception
(`Main.cpp:502-517` rethrows) or is killed, while game and graphics settings
changed in the same session survive because their Back already saved.
`Engine/Source/Ui/Screens/AGENTS.md` `## Standard Menu Contract` lists sound
save among the calls a standard screen makes directly.

Originating gap: a deep analysis of `Engine/Source/Ui` — an
architecture-review finding, independently verified. Unmet criterion: Back on
the Sound screen persists sound settings as Back does on the Game Settings and
Graphics screens.

## Design

The author's recommendation: call `SaveSoundSettings()` in the Back branch of
`SoundMenuScreen::Render`, before the `meUiState` assignment, matching the two
sibling screens. `Ui/SoundSettings.h` is already included
(`SoundMenuScreen.cpp:7`), and `SaveSoundSettings` already opens its own
allocation-tracking suppression (`SoundSettings.cpp:32-33`), so the main-loop
call needs nothing else.

## Critical files

- `Engine/Source/Ui/Screens/SoundMenuScreen.cpp`

## In scope

- The Back branch of `SoundMenuScreen::Render` (`SoundMenuScreen.cpp:68-71`):
  one added call.

## Out of scope

- Saving on any other exit from a settings screen. Leaving any of the three
  screens through the pause input saves nothing for any of them; the sibling
  pattern is "Back saves", and this Plan only completes it.
- The shutdown save at `Main.cpp:531` and `ResetSoundSettings`.
- `SoundSettings.cpp`, the file format, and its version.

## Acceptance criteria

- The diff is the one added call.
- The BrokenEngineSandbox client builds.
- Through `/agent-harness`: open the Sound screen from the pause menu, change
  `Master Volume`, press `Back`, end the client process without a clean quit,
  and relaunch; the Sound screen shows the changed value. The supplied
  evidence records no harness setup, so the executing session drives these
  steps.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  runtime behavior of one screen — when an existing file write happens. The
  file format, version, validation, and trust are unchanged, and sound
  settings are client presentation state outside the CRC, so there is no
  serialization, determinism, wire, replay, threading, or shader exposure.
- This is durability parity for abnormal exits, not a fix for a loss in the
  clean flow: a clean quit already saves.
