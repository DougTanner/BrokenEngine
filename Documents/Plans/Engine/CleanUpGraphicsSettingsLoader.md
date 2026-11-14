<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:58:29.906Z","dependsOn":[]} -->
# Clean Up the Graphics Settings Loader

## Context

`Engine/Source/Ui/GraphicsSettings.cpp` and `GraphicsSettings.h` (client-only)
carry three local leftovers. All three are pre-existing; none changes what a
settings file holds or which files load.

1. Rejected-field names that name no member. `FindInvalidGraphicsSetting`
   returns the first invalid field's name, which `LoadGraphicsSettings` prints
   in the warning at `GraphicsSettings.cpp:270`
   (`"{} rejected: invalid {}"`). Three returned strings match no member of
   `struct GraphicsSettings`: `"eSampleCount"` at `:147` (member
   `vkSampleCount`, `:34`), `"fMaxAnisotropy"` at `:152` (member
   `fMaximumAnisotropy`, `:35`), and `"fMinSampleShading"` at `:157` (member
   `fMinimumSampleShading`, `:36`). Every other string the three settings
   validators return (`FindInvalidGraphicsSetting`, `FindInvalidGameSetting`
   in `GameSettings.cpp`, the sound validator in `SoundSettings.cpp`) equals
   its member's name. The warning is reachable without a hand-edited file:
   narrowing a wrapper's range changes no layout and takes no version bump, so
   a file saved before the narrowing is rejected with this message.
2. An unread result. `LoadGraphicsSettings` is declared `bool`
   (`GraphicsSettings.h:9`) and ends with `return bRead;`
   (`GraphicsSettings.cpp:313`). Its only caller, `Engine/Source/Main.cpp:272`,
   discards the result. The sibling loaders `LoadGameSettings`,
   `LoadSoundSettings`, and `LoadTweaksSettings` return `void`. The local
   `bRead` is still needed at `:264-275`.
3. Two writes that restore a value nothing changes.
   `gWaterShapeDetail.mfCurrent = gWaterShapeDetail.mfDefault;` appears at
   `GraphicsSettings.cpp:262` (in `LoadGraphicsSettings`) and `:329` (in
   `ResetGraphicsSettings`). These are the only writes to the wrapper in
   Engine, Common, DataPacker, Tools, BrokenEngineSandbox, and ModelViewer;
   its readers are `Graphics.cpp:602`, `BufferManager.cpp:775`, and
   `RenderTargetTextures.cpp:41`. Its declaration
   (`GraphicsSettingsWrappersBase.h:29`) says "not exposed or persisted", and
   `Engine/Source/Ui/AGENTS.md` `## Settings Persistence` says the water mesh
   detail is fixed. Line `:262` runs once at startup, before any UI exists, so
   it is dead whatever a game does. Line `:329` is dead because no game writes
   the wrapper: the private game project has no code yet (see `## Design`), so
   the tree searched above holds every write.

Originating gap: a deep analysis of `Engine/Source/Ui` — architecture-review
findings (returned names, unread result) and a refactor finding (dead writes),
each independently verified against the lines above. Unmet criteria: each
returned name is its member's name; the four settings loaders share one
signature; no statement restores a value nothing changes.

## Design

User statement, recorded by the authoring agent from the user's own words
(2026-11-14): asked whether the private game project writes
`gWaterShapeDetail`, the user answered "Private game does not exist any code
yet". Both writes are therefore dead and step 3 removes both.

The author's recommendation:

1. Change the three returned strings to `"vkSampleCount"`,
   `"fMaximumAnisotropy"`, and `"fMinimumSampleShading"`.
2. Declare and define `LoadGraphicsSettings` as `void` and delete
   `return bRead;`. Keep `bRead` as the local it is.
3. Delete lines `:262` and `:329`.

## Critical files

- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/GraphicsSettings.h`

## In scope

- The three string literals returned at `GraphicsSettings.cpp:147`, `:152`,
  and `:157`.
- The return type of `LoadGraphicsSettings` in `GraphicsSettings.h` and
  `GraphicsSettings.cpp`, and its `return bRead;` statement.
- The `gWaterShapeDetail` assignment in `LoadGraphicsSettings` and the one in
  `ResetGraphicsSettings`.

## Out of scope

- `struct GraphicsSettings`, its `static_assert`s, and
  `GraphicsSettings::kiVersion`: no layout or version change.
- Which values `FindInvalidGraphicsSetting` checks and how; see
  `## Coordination`.
- The call at `Main.cpp:272`, which already ignores the result.
- `gWaterShapeDetail` itself, its readers, and the documents that describe it.

## Acceptance criteria

- The diff shows the three corrected names, the `void` loader, and the two
  removed assignments, and nothing else.
- The BrokenEngineSandbox client and ModelViewer build; both compile
  `GraphicsSettings.cpp`.

## Coordination

`Documents/Plans/Engine/DiscreteWrapperAllowedValueCheck.md` rewrites the
checks in `FindInvalidGraphicsSetting`, including the one at `:145-148` whose
returned name this Plan corrects. Neither Plan depends on the other. Whichever
lands second applies its edit on top of the first; in either order the function
ends returning each member's own name.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: the
  signature of a function declared in an engine header changes, which Tier 1's
  "no public signature" excludes. No behavior changes: the file format, the
  validation, the trust in the file, and `kiVersion` are untouched, so there is
  no serialization, determinism/CRC, wire, replay, threading, allocation, or
  shader exposure. Client-only code.
- The private game project has no code yet, so `Main.cpp:272` is the only
  caller of `LoadGraphicsSettings`.
