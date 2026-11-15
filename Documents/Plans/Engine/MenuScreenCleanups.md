<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:37.738Z","dependsOn":[]} -->
# Menu Screen Cleanups: MenuUtils Trims, Sound Back Save, Dead StandardStrings

## Context

A deep analysis of `Engine/Source/Ui` found three independent gaps in the
menu screens, each an independently verified architecture-review or refactor
finding. They share no file; one session runs them together for one build set
and one harness run through the pause menu.

### MenuUtils carries more than any caller uses

Two parts of the `Engine/Source/Ui/MenuUtils.h` interface carry more than any
caller uses.

1. `kfMenuHeadingScale` (`MenuUtils.h:11`) is referenced only as the default of
   `MenuHeading`'s `fHeadingScale` parameter (`MenuUtils.h:99`). All four
   callers pass `kfMainMenuHeadingScale` explicitly:
   `GameSettingsScreen.cpp:42`, `MainMenuScreen.cpp:109`,
   `PauseMenuScreen.cpp:46`, and `SoundMenuScreen.cpp:43`. No caller exists in
   BrokenEngineSandbox or ModelViewer. The layout contract keeps the default
   on purpose today: `Documents/UserInterfaceDesign.txt:243-246` says "Its
   default kfMenuHeadingScale remains available for settings headings",
   `:65-67` calls the four callers "Documented exceptions" to "the larger
   shared settings heading scale", and `:253` lists the constant. So the code
   matches the document; the finding is that a documented shared scale has no
   user.
2. `RadioRow` (`MenuUtils.h:67`, `MenuUtils.cpp:78`) takes both the wrapper
   and `fCurrent`. Of its eight callers, seven pass that same wrapper's
   `mfCurrent` (`GraphicsMenuScreen.cpp:137`, `:146`, `:165`, `:171`, `:179`,
   `:189`, `:200`) and `GameSettingsScreen.cpp:89` passes
   `static_cast<float>(GetUiTheme())`, which equals `gUiTheme.mfCurrent` for
   every allowed value. The function reads the checked state from `fCurrent`
   (`MenuUtils.cpp:117`) and the harness state from `pWrapper->mfCurrent`
   (`:127`): two sources for one value, which a caller could pass out of
   step. Authority: the root KISS directive.

Unmet criteria: `MenuUtils.h` declares no heading scale without a user;
`RadioRow` takes the checked value from the wrapper only.

### The Sound screen's Back does not save

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
save among the calls a standard screen makes directly. This is durability
parity for abnormal exits, not a fix for a loss in the clean flow: a clean quit
already saves.

Unmet criterion: Back on the Sound screen persists sound settings as Back does
on the Game Settings and Graphics screens.

### Nine StandardString values have no caller

`engine::StandardString` (`Engine/Source/Ui/LocalizationBase.h:39-62`) has
nineteen values. Nine have no caller anywhere in Engine, Common, DataPacker,
Tools, BrokenEngineSandbox, or ModelViewer: `kStringComplete`,
`kStringGameOver`, `kStringGamepad`, `kStringMouse`, `kStringMoveWith`,
`kStringOr`, `kStringStart`, `kStringReady`, and `kStringRespawn`
(`LocalizationBase.h:41-55`). A whole-word search for each finds only its
enumerator and the row comment of its translation row in
`LocalizationBase.cpp` (`:9`, `:27`, `:36`, `:63`, `:72`, `:90`, `:117`,
`:126`, `:135`). The other ten values each have callers. Nothing indexes the
enum generically except the uppercase loop in `InitializeLocalization`.

The game facade re-exports the enum, so a game reaches these names through its
own header. The nine are gameplay and input-prompt text, the kind a game HUD
shows. `Engine/Source/Ui/AGENTS.md` `## Localization` describes
`StandardString` as holding "the standard menu strings", which the nine are
not, and `Engine/Source/AGENTS.md` `## Hub Conventions` wants an engine-owned
enum value that names a game concept recorded as deliberate ownership in a
leaf document. The ten values with callers are all menu strings, so deleting
the nine makes that description correct as written.

Unmet criterion: every `StandardString` value either has a caller or is
recorded as deliberately engine-owned for games.

### Caller completeness

`MenuHeading`, `RadioRow`, and `StandardString` are public and a game may use
them. The private game project has no code yet (the user's statement,
2026-11-14), so the call sites and searches above are the complete set.

## Design

User decisions, recorded by the authoring agent from the user's own words
(2026-11-14): asked whether to remove the unused `MenuHeading` default and
`kfMenuHeadingScale`, which reverses the statement at
`Documents/UserInterfaceDesign.txt:243-246` that the default remains
available, the user answered "Remove it" (step 1). Asked what to do with the
nine `StandardString` values, the user answered "Delete them" (step 4). Steps
2 and 3 are the author's recommendations.

1. Delete `kfMenuHeadingScale` and make `MenuHeading`'s `fHeadingScale` a
   required parameter. Update the comment above `MenuHeading` and
   `Documents/UserInterfaceDesign.txt` at `:65-67`, `:243-246`, and `:253` so
   the contract no longer describes a larger shared settings heading scale or
   a default. Rationale: a default no screen uses is the speculative leftover
   the root Directives reject.
2. Remove `RadioRow`'s `fCurrent` parameter. Inside the function, copy
   `pWrapper->mfCurrent` into a local at entry and compare against that local
   in the loop, so the checked option drawn on a click frame is the pre-click
   value as it is today (`Set` at `MenuUtils.cpp:119` changes `mfCurrent`
   mid-loop). Update the eight call sites and the comment above the
   declaration.
3. Call `SaveSoundSettings()` in the Back branch of `SoundMenuScreen::Render`,
   before the `meUiState` assignment, matching the two sibling screens.
   `Ui/SoundSettings.h` is already included (`SoundMenuScreen.cpp:7`), and
   `SaveSoundSettings` already opens its own allocation-tracking suppression
   (`SoundSettings.cpp:32-33`), so the main-loop call needs nothing else.
4. Delete the nine enumerators from `StandardString` and their nine rows, with
   the row comments, from `sppTranslatedStrings` in `LocalizationBase.cpp`. The
   `static_assert` at `LocalizationBase.cpp:194` catches a mismatch between the
   two. The enum is not persisted and its values are never sent, so
   renumbering the rest is safe.

## Critical files

- `Engine/Source/Ui/MenuUtils.h`
- `Engine/Source/Ui/MenuUtils.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Ui/Screens/GameSettingsScreen.cpp`
- `Engine/Source/Ui/Screens/SoundMenuScreen.cpp`
- `Engine/Source/Ui/LocalizationBase.h`
- `Engine/Source/Ui/LocalizationBase.cpp`
- `Documents/UserInterfaceDesign.txt`

## In scope

- MenuUtils: `kfMenuHeadingScale`, the default argument of `MenuHeading`, and
  the comment above `MenuHeading`; the three passages of
  `Documents/UserInterfaceDesign.txt` named in Design step 1; `RadioRow`'s
  declaration, definition, declaration comment, and its eight call sites.
- Sound screen: the Back branch of `SoundMenuScreen::Render`
  (`SoundMenuScreen.cpp:68-71`), one added call.
- Localization: the nine enumerators named in `## Context` and their nine rows
  in `sppTranslatedStrings`.

## Out of scope

- `kfMainMenuHeadingScale` and the four `MenuHeading` call sites, which
  already pass it.
- The option labels and headers passed to `RadioRow`; they are harness names.
- The other shared layout constants in `MenuUtils.h`, which
  `Documents/UserInterfaceDesign.txt` section `4. Shared Layout Constants`
  places there deliberately.
- `WrapperToggle`, `WrapperSlider`, `WrapperPlusMinus`, `DrawPanelAccents`,
  and `MenuButton`.
- Saving on any other exit from a settings screen. Leaving any of the three
  screens through the pause input saves nothing for any of them; the sibling
  pattern is "Back saves", and this Plan only completes it.
- The shutdown save at `Main.cpp:531` and `ResetSoundSettings`.
- `SoundSettings.cpp`, the sound settings file format, and its version.
- The ten `StandardString` values with callers, the `Language` enum,
  `TranslatedString`, and `InitializeLocalization`.
- The translations themselves.
- The game facade headers.
- `Engine/Source/Ui/AGENTS.md`, whose `## Localization` wording the deletion
  makes correct.

## Risk tier

Tier 2 (the authors' classification). Triggers: two public signatures in a
shared engine header change (`MenuHeading`, `RadioRow`), and values are removed
from an enum declared in an engine header that game code re-exports — public
interface changes Tier 1 excludes; plus scoped runtime behavior of one screen,
when an existing file write happens.

Invariants:

- `MenuHeading` and `RadioRow` behavior is preserved, including the pre-click
  checked option drawn on a click frame.
- The sound settings file format, version, validation, and trust are
  unchanged; sound settings are client presentation state outside the CRC.
- No determinism/CRC, wire, serialization, replay, threading, allocation, or
  shader exposure. Build exposure: `MenuUtils.h` compiles into both builds.

## Acceptance criteria

- The diff shows exactly: the removed `kfMenuHeadingScale` and `MenuHeading`
  default, the three document passages, and `RadioRow` without `fCurrent` at
  its eight call sites; the one added `SaveSoundSettings()` call in the Sound
  screen's Back branch; and the nine removed enumerators with their nine
  `sppTranslatedStrings` rows.
- The BrokenEngineSandbox client and server and ModelViewer build.
- Through `/agent-harness`, in one client session (the supplied evidence
  records no harness setup, so the executing session drives these steps; the
  Graphics radio-row baseline is captured before the change):
  - `describe_ui` on the main menu and on the pause menu shows their labels in
    the selected language.
  - On the Graphics screen, opened from the pause menu, `describe_ui` reports
    the same checked option for each radio row before and after the change,
    and a `click` on another option of one row moves the checked state to it.
  - Open the Sound screen from the pause menu, change `Master Volume`, press
    `Back`, end the client process without a clean quit, and relaunch; the
    Sound screen shows the changed value.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
