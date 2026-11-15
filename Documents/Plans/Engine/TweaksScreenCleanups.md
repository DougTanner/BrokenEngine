<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:00:24.543Z","dependsOn":[]} -->
# Tweaks Screen Cleanups

## Context

Three gaps in the engine Tweaks screen, each a deep-analysis finding on
`Engine/Source/Ui` that was independently verified. `TS/` below stands for
`Engine/Source/Ui/Screens/TweaksScreen/`.

### Water and Pbr sections break the wrapper contracts

Three places in the Tweaks Water and Pbr sections break contracts the UI
documents state.

1. The wave-count row. `TweaksScreen::RenderWaveCountRadioButtons`
   (`TS/TweaksScreen.h:76`, `TS/TweaksScreen.cpp:71-90`) is a Water-only widget
   declared on the generic host; its only callers are
   `TS/TweaksScreenWater.cpp:98` (`gWaterLowCount`) and `:116`
   (`gWaterMediumCount`). It has three faults:
   - It returns early while any slider is active (`TweaksScreen.cpp:73-76`),
     so the row disappears and every control below it moves up. The other
     Tweaks widgets keep their layout slot by drawing at alpha 0:
     `WrapperSlider` (`:149-153`), `ChevronIndexSelector` (`:94-99`), and
     `WrapperSeparatorText` (`:326-329`). The code comments at
     `TweaksScreen.cpp:94` and `:148` state the intent: a widget keeps its
     layout slot while another slider is active. `TS/AGENTS.md`
     `## Registration and Layout Contracts` says only that, while a slider is
     dragged, the rest of the toggle bar keeps its layout while drawn
     invisibly and saved window layouts stay stable. Dragging any slider in
     the Low or Medium tab therefore moves the rows under the cursor.
   - It restates the allowed set as the literal list
     `{15, 31, 63, 127, 255}` (`TweaksScreen.cpp:81`), a third copy of the
     sets at `Engine/Source/Ui/WaterWrappersBase.h:83` and `:95`.
     `Wrapper::Set<T>` asserts through `GetIndex()` on a value outside
     `mAllowed`, so a mismatch is an assert.
   - Its text label is "Wave Count" (`TweaksScreen.cpp:78`), which spells
     neither wrapper.
2. Six Water labels do not spell their wrapper, against the label rule in
   `Engine/Source/Ui/AGENTS.md` `## Wrapper Contracts`:
   `TS/TweaksScreenWater.cpp:28`, `:38`, `:48` ("Sample 1", "Sample 2",
   "Sample 3" for `gWaterNormalIndexOne`, `Two`, `Three`), `:179` ("Foam
   Enabled" for `gWaterFoamEnabled`), `:184` ("Foam Texture" for
   `gWaterFoamTextureIndex`), and `:188` ("Foam Breakup Texture" for
   `gWaterFoamBreakupTextureIndex`). No file under `.agents`, `Documents`,
   `Tools`, or `Projects` uses the old labels.
3. Declaration order differs from slider order. `TS/AGENTS.md` says wrapper
   declaration order and each section's slider order stay aligned.
   `Engine/Source/Ui/PbrWrappersBase.h:14-18` declares `gPbrExposure`,
   `gPbrGamma`, and the three `gColorGrading*` wrappers between the Brdf group
   (`:10-13`) and the Lighting group (`:19-22`), but `TS/TweaksScreenPbr.cpp`
   draws them last (`:65-71`, the Post Processing sub-tab), after
   `gPbrEmissive` (`:55`, declared at header `:33`).

### The drag-hide state is entered without a drag

While a Tweaks slider is dragged, the screen hides everything but the owning
section (`TS/AGENTS.md` `## Registration and Layout Contracts`, last bullet),
and `RenderUiScreens` hides the other screens (`UiScreens.cpp:50-54`). The
state is `TweaksScreen::mActiveSlider`. It is entered and left on different
conditions:

- It is set whenever the slider item is active:
  `if (ImGui::IsItemActive())` in `TweaksScreen::WrapperSlider`
  (`TS/TweaksScreen.cpp:178-182`) and for the Sun Angle slider in
  `RenderToggleBar` (`:305-309`).
- It is cleared at the top of `Render` whenever the left mouse button is up
  (`:209-212`).

A slider can be active with the left button up. The discrete slider passes
`ImGuiSliderFlags_NoInput` (`:169`); the float slider (`:174`) and Sun Angle
(`:304`) do not, and Dear ImGui 1.93.1 WIP `SliderScalar`
(`imgui_widgets.cpp:3381-3409`) makes such a slider active for Ctrl+Click typed
entry and for keyboard or gamepad activation, with no mouse button held. The
harness reaches the same state on purpose: `set_slider` activates the item with
`ImGuiActivateFlags_PreferInput` to open its text input
(`Engine/Source/Agent/AgentInput.cpp:76-98`).

In that state, each frame: `Render` clears the name; `RenderToggleBar` sees no
active slider and draws the toggle bar normally; the section loop draws every
section before the owning one normally; inside the owning section the controls
before the active slider draw normally, the slider sets the name, and every
later slider, separator, and chevron in that section draws at alpha 0; every
later section's window is skipped (`:223-236`); and `RenderUiScreens` returns
early, hiding the modal and the other screens. The result is a half-hidden
screen for the whole typed entry, where the contract describes one
all-or-nothing hide "while a slider is dragged". No harness run recorded it;
the sequence is established by code reading.

A third set site has the same asymmetry: `TS/TweaksScreenLighting.cpp:114-118`
sets the name whenever `CurveWidget` reports interaction, which includes the
frame of a right-click point removal with the left button up
(`Engine/Source/Ui/CurveWidget.cpp:103-107`).

### Unsaved sections ignore their staggered X

The `TweaksScreen` constructor (`TS/TweaksScreen.cpp:60-68`) gives each section
a default window position of `(kfStartX + i * kfOffsetX) * UiScale()` with Y 0,
under the comment "Zero Y defers vertical positioning to mfToggleBarBottom; X
offsets stagger the windows."

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
it, and `TS/AGENTS.md` `## Registration and Layout Contracts` says a CRC
mismatch "keeps constructor defaults". Current code behavior (all sections
stacked at one X) is the side not trusted.

### Unmet criteria

- The wave-count row keeps its layout slot while a slider is active and lists
  exactly its wrapper's allowed values; each Tweaks control's label is its
  identifier's words in order; header order equals slider order.
- The hide state is entered only by a drag and is never partial.
- A section without a saved position opens at its constructor X.

## Design

The author's recommendation, in order:

1. Remove `RenderWaveCountRadioButtons` from `TweaksScreen`. Add one
   file-static function in `TweaksScreenWater.cpp` that takes the screen, a
   label, and the count wrapper; draws the label and one radio button per
   entry of the wrapper's `mAllowed`, labelled with that value so the buttons
   stay "15" through "255"; writes a click through `SetIndex`; and hides the
   way `ChevronIndexSelector` does — push alpha 0 while
   `rScreen.mActiveSlider` is not empty and ignore a click in that state. Call
   it with "Water Low Count" and "Water Medium Count". Rationale: copying
   `ChevronIndexSelector`'s alpha push and click guard needs no new assumption
   about ImGui behavior. The function only reads `mActiveSlider` and adds no
   site that sets it.
2. Rename the six Water labels to "Water Normal Index One", "Water Normal
   Index Two", "Water Normal Index Three", "Water Foam Enabled", "Water Foam
   Texture Index", and "Water Foam Breakup Texture Index".
3. Move the five lines `PbrWrappersBase.h:14-18`, with the `gPbrGamma`
   comment, to the end of the wrapper list. Rationale: moving the sub-tab
   instead would renumber a persisted sub-tab index; the initializers are
   literals and nothing iterates the wrappers by declaration order, so the
   header move has no runtime effect.
4. Make each `mActiveSlider` set site require the condition the reset already
   uses — the left mouse button is down — so the name is set only when the
   item is active and that button is held, at `TweaksScreen.cpp:178`, `:305`,
   and `TweaksScreenLighting.cpp:114`. These three stay the complete list of
   set sites. Rationale: it matches the "dragged" wording of the contract and
   of the comment at `UiScreens.cpp:50`, needs no new assumption about ImGui
   behavior, and leaves the screen fully visible during typed entry, gamepad
   adjustment, and harness `set_slider`.

   Not recommended: passing `ImGuiSliderFlags_NoInput` to the float sliders.
   It would make the harness reject `set_slider` on them
   (`AgentInput.cpp:78-82` requires the inputable flag), it removes typed
   entry, and it does not block the gamepad path.
5. In `RenderSectionWindow`, take the fallback X from
   `mpWindowPositions[iSection].x` and keep `mfToggleBarBottom` as the
   fallback Y, and delete the function-local `kfStartX`. Rationale: one
   expression makes the documented default real, with no new state.

   This is a visible behavior change: sections that open without a saved
   position are offset from each other horizontally instead of stacked.

   The alternative — delete the constructor loop's X term, `kfOffsetX`, and
   the comment's second sentence, and correct the Tweaks document — is right
   only if the user says stacking is intended; it is not what this Plan does.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`
- `Engine/Source/Ui/PbrWrappersBase.h`

## In scope

- `TweaksScreen::RenderWaveCountRadioButtons`: its declaration and definition,
  removed.
- `TweaksScreenWater.cpp`: one new file-static function, the two call sites,
  and the six label literals.
- `PbrWrappersBase.h`: the position of five wrapper declarations.
- The condition guarding the `mActiveSlider` and `miActiveSliderSection`
  assignments in `TweaksScreen::WrapperSlider`, in
  `TweaksScreen::RenderToggleBar` (Sun Angle), and after the `CurveWidget`
  call in `RenderLightingSection`.
- `TweaksScreen::RenderSectionWindow`: the `f2InitialPosition` expression and
  the function-local `kfStartX` (`:385-386`).

## Out of scope

- The `mActiveSlider` reset at `TweaksScreen.cpp:209-212` and every existing
  reader of `mActiveSlider`, including `UiScreens.cpp`.
- Slider flags, and making typed entry hide the screen like a drag.
- `CurveWidget` and `AgentInput`.
- Reusing `MenuUtils` `RadioRow` for the wave-count row.
- The allowed sets in `WaterWrappersBase.h` and any shader array they index.
- Every other Tweaks label, and the player-facing menu labels.
- The Pbr section's sub-tabs and slider order.
- The constructor, `kfStartX` and `kfOffsetX` there, and the stagger amount.
- `SaveState`, `LoadState`, the persisted layout, and
  `TweaksSettings::kiVersion`.
- `ImGuiCond_FirstUseEver` and ImGui settings-file persistence.
- The Tweaks documents, which already say "dragged".
- Game Tweaks content.

## Risk

- Change Workflow tier: Tier 2. Trigger: scoped runtime behavior of one
  developer screen — a layout change during a drag, renamed labels (which are
  harness item names), what `RenderUiScreens` draws while the screen is used,
  and where a window first appears — plus the removal of a public member.
- Client-only, `kbDebugInput` builds only. No determinism/CRC, wire,
  serialization, replay, threading, allocation, or shader exposure; the
  persisted Tweaks layout and its format are untouched.

## Acceptance criteria

- The diff shows: the member gone, the file-static reading `mAllowed`, the six
  renamed labels plus the two row labels, and the five moved declarations; the
  three set sites conditioned on the left button being down; the one changed
  `RenderSectionWindow` expression and the removed constant; and no other
  change.
- The BrokenEngineSandbox client and ModelViewer build.
- One `/agent-harness` run: launch the client with an empty app-data
  directory and show Tweaks. The supplied evidence records no harness setup.
  - Open two sections, the Water section among them, and capture a
    `screenshot`; the two section windows start at different X positions below
    the toggle bar.
  - With the Water section open, `describe_ui` lists the renamed labels and
    the wave-count radio buttons, and after a `click` on another count a
    `screenshot` shows that count's radio button selected. `describe_ui`
    reports `checked` only for checkboxes and `MenuUtils` `RadioRow` options,
    and no harness command reads the count wrapper, so the screenshot is the
    runtime observable; that the click writes the wrapper through `SetIndex`
    is settled by code reading.
  - `set_slider` on a Tweaks float slider (for example `Water Scale` in the
    Water section) still succeeds and `describe_ui` reads back the new value.
- Settled by code reading, because the harness drives the UI by item
  activation and cannot hold a mouse drag or button:
  - The wave-count row keeps its layout slot while a slider is active.
  - The hide state is no longer entered without a drag; the half-hidden frames
    last only while the text input is open.

## Notes

- The renamed chevron labels are longer, so the two-column Specular table
  shows longer text in its first column.
- A caller of `RenderWaveCountRadioButtons` in the private game project cannot
  be searched; that project's source is absent from this machine.
- If the user wants typed entry to hide the screen like a drag instead, the
  reset would have to key on "no item active", which depends on Dear ImGui
  behavior not yet verified: whether `ImGui::IsAnyItemActive()` at the top of a
  frame still reports a slider in text input. That is a different change and
  outside this Plan.
- The practical gain of the staggered X is modest: a small scaled X offset per
  section at one shared Y.
