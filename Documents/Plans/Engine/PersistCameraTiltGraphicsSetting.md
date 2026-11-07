<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:38.622Z","dependsOn":[]} -->
# Persist Camera Tilt in the graphics settings

## Context

An audit of commits cfd6116e, 13e4ae19, and 5c2e6a8d found one required defect
outside that session's documentation change. Line numbers below are from
5c2e6a8d.

Camera Tilt is a player-facing Graphics control
(`Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:129`), beside Field of View,
but `Engine/Source/Ui/GraphicsSettings.cpp` never references `gCameraTilt`. So
a tilt the player sets is lost on restart, and Defaults leaves it in place,
while `gFieldOfView` beside it saves (`GraphicsSettings.cpp:101`), validates,
loads, and resets (`:277`). The wrapper is declared at
`Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp:16`, and its one reader is
`CameraBase.cpp:477`.

Root cause: the control was added to the menu without a matching field in the
`GraphicsSettings` POD or a line in `ResetGraphicsSettings`.

## Design

Mirror `gFieldOfView` through the settings path. The author recommends a new
`float fCameraTilt` as the last `GraphicsSettings` member, because the existing
offset checks state that existing setting bytes do not move. The other option
the audit named, leaving the slider in Tweaks only, would remove a control the
player already has.

The file format changes, so `GraphicsSettings::kiVersion` is bumped. Older files
fail the current-format gate and fall back to defaults, which is the existing
behavior; no migration is added.

## Critical files

- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/AGENTS.md`

## In scope

- `GraphicsSettings`: the new member, `kiVersion`, a `BT_OFFSETOF` check for it,
  and the `sizeof` check.
- `SaveGraphicsSettings`: write `gCameraTilt.mfCurrent`.
- `FindInvalidGraphicsSetting`: reject a value outside
  `gCameraTilt.IsInRange`, as `fFieldOfView` does.
- `LoadGraphicsSettings`: apply it with `gCameraTilt.Set`.
- `ResetGraphicsSettings`: restore `gCameraTilt.mfDefault`.
- `Engine/Source/Ui/AGENTS.md` `## Settings Persistence`: the sentence listing
  what `GraphicsSettings.bin` persists, and the version it states.

## Out of scope

- The wrapper's default or range, and the Graphics and Tweaks sliders.
- Any other wrapper's persistence.
- Migration of older `GraphicsSettings.bin` files.
- `Engine/Source/Graphics/CameraBase.cpp`.

## Risk triggers and invariants

- Tier 3. Trigger: serialization and save compatibility, since the change
  alters the layout of `GraphicsSettings.bin`.
- Client only. `gCameraTilt` is render state; it reaches no Frame, CRC, wire,
  replay, or `.pack` data.
- A loader rejects the whole file on one bad value before applying any
  (`Engine/Source/Ui/AGENTS.md` `## Settings Persistence`).
- No allocation or threading change. The GraphicsMenu and shutdown call sites of
  `SaveGraphicsSettings` stay as they are.

## Acceptance criteria

- `/compile` builds the BrokenEngineSandbox client and server.
- A live `/agent-harness` run: set Camera Tilt away from its default with
  `set_slider`, quit the client in order, relaunch, and, with the Graphics
  screen open, `describe_ui` reports the set value as the `value` of the
  `##Camera Tilt` item in window `GraphicsMenu`, text with two decimals, so the
  set value has at most two decimals. The audit recorded this scenario from
  reading the code; the supplied evidence records no harness run or setup.
- In the same run, after the Graphics screen's Defaults button, with the
  Graphics screen open, `describe_ui` reports the wrapper default as the
  `value` of the `##Camera Tilt` item in window `GraphicsMenu`, text with two
  decimals.
- A `GraphicsSettings.bin` of the previous version is rejected and defaults
  load; an out-of-range tilt in a current-version file rejects the whole file.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

`Documents/Plans/Engine/ObjectShadowCadenceAndBlurSetting.md` also changes the
`GraphicsSettings` layout and version. Either order works: the later landing
places its members after the earlier one's and bumps `kiVersion` again.
