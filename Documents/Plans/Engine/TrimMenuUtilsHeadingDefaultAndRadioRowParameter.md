<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:37.738Z","dependsOn":[]} -->
# Trim an Unused Default and a Redundant Parameter From MenuUtils

## Context

Two parts of the `Engine/Source/Ui/MenuUtils.h` interface carry more than any
caller uses.

1. `kfMenuHeadingScale` (`MenuUtils.h:11`) is referenced only as the default of
   `MenuHeading`'s `fHeadingScale` parameter (`MenuUtils.h:99`). All four
   callers pass `kfMainMenuHeadingScale` explicitly:
   `GameSettingsScreen.cpp:42`, `MainMenuScreen.cpp:109`,
   `PauseMenuScreen.cpp:46`, and `SoundMenuScreen.cpp:43`. No caller exists in
   BrokenEngineSandbox or ModelViewer. The layout contract keeps the default
   on purpose today: `Documents/UserInterfaceDesign.txt:243-246` says "Its
   default kfMenuHeadingScale remains available for settings headings", `:65-67`
   calls the four callers "Documented exceptions" to "the larger shared
   settings heading scale", and `:253` lists the constant. So the code matches
   the document; the finding is that a documented shared scale has no user.
2. `RadioRow` (`MenuUtils.h:67`, `MenuUtils.cpp:78`) takes both the wrapper
   and `fCurrent`. Of its eight callers, seven pass that same wrapper's
   `mfCurrent` (`GraphicsMenuScreen.cpp:137`, `:146`, `:165`, `:171`, `:179`,
   `:189`, `:200`) and `GameSettingsScreen.cpp:89` passes
   `static_cast<float>(GetUiTheme())`, which equals `gUiTheme.mfCurrent` for
   every allowed value. The function reads the checked state from `fCurrent`
   (`MenuUtils.cpp:117`) and the harness state from `pWrapper->mfCurrent`
   (`:127`): two sources for one value, which a caller could pass out of
   step. Authority: the root KISS directive.

Originating gap: a deep analysis of `Engine/Source/Ui` — an
architecture-review finding and a refactor finding, each independently
verified. Unmet criteria: `MenuUtils.h` declares no heading scale without a
user; `RadioRow` takes the checked value from the wrapper only.

## Design

User decision, recorded by the authoring agent from the user's own words
(2026-11-14): asked whether to remove the unused `MenuHeading` default and
`kfMenuHeadingScale`, which reverses the statement at
`Documents/UserInterfaceDesign.txt:243-246` that the default remains
available, the user answered "Remove it". Step 1 carries that decision,
including the document edit; step 2 is the author's recommendation.

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

## Critical files

- `Engine/Source/Ui/MenuUtils.h`
- `Engine/Source/Ui/MenuUtils.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Ui/Screens/GameSettingsScreen.cpp`
- `Documents/UserInterfaceDesign.txt`

## In scope

- `kfMenuHeadingScale`, the default argument of `MenuHeading`, and the comment
  above `MenuHeading`.
- The three passages of `Documents/UserInterfaceDesign.txt` named in Design
  step 1.
- `RadioRow`'s declaration, definition, declaration comment, and its eight
  call sites.

## Out of scope

- `kfMainMenuHeadingScale` and the four `MenuHeading` call sites, which
  already pass it.
- The option labels and headers passed to `RadioRow`; they are harness names.
- The other shared layout constants in `MenuUtils.h`, which
  `Documents/UserInterfaceDesign.txt` section `4. Shared Layout Constants`
  places there deliberately.
- `WrapperToggle`, `WrapperSlider`, `WrapperPlusMinus`, `DrawPanelAccents`,
  and `MenuButton`.

## Acceptance criteria

- The diff shows the removed constant and default, the three document
  passages, and `RadioRow` without `fCurrent` at its eight call sites.
- The BrokenEngineSandbox client and server and ModelViewer build.
- Through `/agent-harness`, on the Graphics screen `describe_ui` reports the
  same checked option for each radio row before and after the change, and a
  `click` on another option of one row moves the checked state to it. The
  supplied evidence records no harness setup; opening the Graphics screen from
  the pause menu serves.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: two
  public signatures in a shared engine header change, which Tier 1 excludes.
  Behavior is preserved. No determinism/CRC, wire, serialization, replay,
  threading, allocation, or shader exposure. Build exposure: `MenuUtils.h`
  compiles into both builds.
- Both functions are public and a game may call them. The private game project
  has no code yet (the user's statement, 2026-11-14), so the call sites in
  `## Context` are the complete set.
