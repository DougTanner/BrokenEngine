<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:00:24.543Z","dependsOn":[]} -->
# Open Unsaved Tweaks Sections at Their Staggered X

## Context

The `TweaksScreen` constructor
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:60-68`) gives each
section a default window position of
`(kfStartX + i * kfOffsetX) * UiScale()` with Y 0, under the comment "Zero Y
defers vertical positioning to mfToggleBarBottom; X offsets stagger the
windows."

`RenderSectionWindow` never uses that X. It reads the stored position only
when its Y is above 0, and otherwise builds the position from a second,
function-local `kfStartX` (`:385-386`):
`ImVec2 {kfStartX * UiScale(), mfToggleBarBottom}`. Line `:396` then
overwrites the stored slot with the window's real position. The other readers
only carry the value (`SaveState` `:346-347`, `LoadState` `:367`). So the
staggered X, `kfOffsetX`, and the constructor loop's X term are dead, and
every section without a saved position opens on the same spot.

That fallback decides more than a first launch: ImGui's own settings file is
off (`Engine/Source/Graphics/Managers/ImGuiManager.cpp:106` sets `IniFilename`
to null), so `ImGuiCond_FirstUseEver` at `:387` applies on every launch, and
the fallback places every section whose saved Y is 0 — a fresh app-data
directory, a section never opened, or a layout discarded by the CRC gate in
`LoadState`.

Sources disagree, and this Plan trusts the documents over the code: the
constructor comment states the stagger, a constant and a loop exist only for
it, and `Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md`
`## Registration and Layout Contracts` says a CRC mismatch "keeps constructor
defaults". Current code behavior (all sections stacked at one X) is the side
not trusted.

Originating gap: a deep analysis of `Engine/Source/Ui` — a refactor finding,
independently verified. Unmet criterion: a section without a saved position
opens at its constructor X.

## Design

The author's recommendation: in `RenderSectionWindow`, take the fallback X
from `mpWindowPositions[iSection].x` and keep `mfToggleBarBottom` as the
fallback Y, and delete the function-local `kfStartX`. Rationale: one
expression makes the documented default real, with no new state.

This is a visible behavior change: sections that open without a saved position
are offset from each other horizontally instead of stacked.

The alternative — delete the constructor loop's X term, `kfOffsetX`, and the
comment's second sentence, and correct the Tweaks document — is right only if
the user says stacking is intended; it is not what this Plan does.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`

## In scope

- `TweaksScreen::RenderSectionWindow`: the `f2InitialPosition` expression and
  the function-local `kfStartX` (`:385-386`).

## Out of scope

- The constructor, `kfStartX` and `kfOffsetX` there, and the stagger amount.
- `SaveState`, `LoadState`, the persisted layout, and
  `TweaksSettings::kiVersion`.
- `ImGuiCond_FirstUseEver` and ImGui settings-file persistence.
- Any other part of `TweaksScreen.cpp`; see `## Coordination`.

## Acceptance criteria

- The diff is the one changed expression and the removed constant.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`: launch the client with an empty app-data
  directory, show Tweaks, open two sections, and capture a screenshot; the two
  section windows start at different X positions below the toggle bar. The
  supplied evidence records no harness setup.

## Coordination

Two other Plans edit the same file, neither of them in `RenderSectionWindow`.
Neither depends on this Plan and it depends on neither; whichever lands second
re-derives its region by symbol from the landed file.

- `Documents/Plans/Engine/AlignTweaksWaterAndPbrWithWrapperContracts.md`
  removes `TweaksScreen::RenderWaveCountRadioButtons`
  (`TweaksScreen.cpp:71-90`).
- `Documents/Plans/Engine/TweaksDragHideRequiresLeftButton.md` conditions the
  `mActiveSlider` set sites in `TweaksScreen::WrapperSlider` (`:178`) and
  `TweaksScreen::RenderToggleBar` (`:305`).

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  runtime behavior of one developer screen — where a window first appears.
  Client-only, `kbDebugInput` builds only. The persisted layout format is
  unchanged, so there is no serialization exposure, and no determinism/CRC,
  wire, replay, threading, allocation, or shader exposure.
- The practical gain is modest: a small scaled X offset per section at one
  shared Y.
