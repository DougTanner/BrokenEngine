<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:00:04.148Z","dependsOn":[]} -->
# Align the Tweaks Water and Pbr Sections With the Wrapper Contracts

## Context

Three places in the engine Tweaks Water and Pbr sections break contracts the
UI documents state. `TS/` below stands for
`Engine/Source/Ui/Screens/TweaksScreen/`.

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

Originating gap: a deep analysis of `Engine/Source/Ui` — an
architecture-review finding (the wave-count row) and two refactor findings
(labels, order), each independently verified. Unmet criteria: the wave-count
row keeps its layout slot while a slider is active and lists exactly its
wrapper's allowed values; each Tweaks control's label is its identifier's
words in order; header order equals slider order.

## Design

The author's recommendation:

1. Remove `RenderWaveCountRadioButtons` from `TweaksScreen`. Add one
   file-static function in `TweaksScreenWater.cpp` that takes the screen, a
   label, and the count wrapper; draws the label and one radio button per
   entry of the wrapper's `mAllowed`, labelled with that value so the buttons
   stay "15" through "255"; writes a click through `SetIndex`; and hides the
   way `ChevronIndexSelector` does — push alpha 0 while
   `rScreen.mActiveSlider` is not empty and ignore a click in that state. Call
   it with "Water Low Count" and "Water Medium Count". Rationale: copying
   `ChevronIndexSelector`'s alpha push and click guard needs no new assumption
   about ImGui behavior.
2. Rename the six labels to "Water Normal Index One", "Water Normal Index
   Two", "Water Normal Index Three", "Water Foam Enabled", "Water Foam Texture
   Index", and "Water Foam Breakup Texture Index".
3. Move the five lines `PbrWrappersBase.h:14-18`, with the `gPbrGamma`
   comment, to the end of the wrapper list. Rationale: moving the sub-tab
   instead would renumber a persisted sub-tab index; the initializers are
   literals and nothing iterates the wrappers by declaration order, so the
   header move has no runtime effect.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`
- `Engine/Source/Ui/PbrWrappersBase.h`

## In scope

- `TweaksScreen::RenderWaveCountRadioButtons`: its declaration and definition,
  removed.
- `TweaksScreenWater.cpp`: one new file-static function, the two call sites,
  and the six label literals.
- `PbrWrappersBase.h`: the position of five wrapper declarations.

## Out of scope

- `mActiveSlider`, where it is set and cleared, and every other hide path; see
  `## Coordination`.
- Reusing `MenuUtils` `RadioRow` for the wave-count row.
- The allowed sets in `WaterWrappersBase.h` and any shader array they index.
- Every other Tweaks label, and the player-facing menu labels.
- The Pbr section's sub-tabs and slider order.
- Game Tweaks content.

## Acceptance criteria

- The diff shows the member gone, the file-static reading `mAllowed`, the six
  renamed labels plus the two row labels, and the five moved declarations.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness` with Tweaks shown and the Water section open,
  `describe_ui` lists the renamed labels and the wave-count radio buttons, and
  after a `click` on another count a `screenshot` shows that count's radio
  button selected. `describe_ui` reports `checked` only for checkboxes and
  `MenuUtils` `RadioRow` options, and no harness command reads the count
  wrapper, so the screenshot is the runtime observable; that the click writes
  the wrapper through `SetIndex` is settled by code reading. The supplied
  evidence records no harness setup.
- The layout-slot criterion is settled by code reading: the harness drives the
  UI by item activation and cannot hold a mouse drag, so it cannot show the
  row during one.

## Coordination

`Documents/Plans/Engine/TweaksDragHideRequiresLeftButton.md` changes the
conditions under which `mActiveSlider` is set (`TweaksScreen.cpp:178` and
`:305`, `TweaksScreenLighting.cpp:114-118`). Neither Plan depends on the other.
This Plan's new function only reads `mActiveSlider` and adds no site that sets
it, so it hides correctly under either set condition; whichever Plan lands
second applies its edit on top of the first.

One more Plan edits a file this Plan edits. It does not depend on this Plan
and this Plan does not depend on it; whichever lands second re-derives its
regions by symbol from the landed files.

- `Documents/Plans/Engine/TweaksDefaultWindowUsesStaggeredX.md` edits
  `TweaksScreen::RenderSectionWindow` (`TweaksScreen.cpp:385-386`), a
  different function from the one this Plan removes.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  runtime behavior of one developer screen — a layout change during a drag and
  renamed labels, which are harness item names — plus the removal of a public
  member. Client-only, `kbDebugInput` builds only. No determinism/CRC, wire,
  serialization, replay, threading, allocation, or shader exposure; the
  persisted Tweaks layout is untouched.
- The renamed chevron labels are longer, so the two-column Specular table
  shows longer text in its first column.
- A caller of `RenderWaveCountRadioButtons` in the private game project cannot
  be searched; that project's source is absent from this machine.
