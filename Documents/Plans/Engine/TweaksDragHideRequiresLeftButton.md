<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:00:10.834Z","dependsOn":[]} -->
# Enter the Tweaks Drag-Hide State Only During a Left-Button Drag

## Context

While a Tweaks slider is dragged, the screen hides everything but the owning
section (`Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md`
`## Registration and Layout Contracts`, last bullet), and
`RenderUiScreens` hides the other screens (`UiScreens.cpp:50-54`). The state is
`TweaksScreen::mActiveSlider`. `TS/` below stands for
`Engine/Source/Ui/Screens/TweaksScreen/`.

The state is entered and left on different conditions:

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

Originating gap: a deep analysis of `Engine/Source/Ui` — a refactor finding,
independently verified. Unmet criterion: the hide state is entered only by a
drag and is never partial.

## Design

The author's recommendation: make each set site require the condition the
reset already uses — the left mouse button is down — so the name is set only
when the item is active and that button is held, at `TweaksScreen.cpp:178`,
`:305`, and `TweaksScreenLighting.cpp:114`. Rationale: it matches the "dragged"
wording of the contract and of the comment at `UiScreens.cpp:50`, needs no new
assumption about ImGui behavior, and leaves the screen fully visible during
typed entry, gamepad adjustment, and harness `set_slider`.

Not recommended: passing `ImGuiSliderFlags_NoInput` to the float sliders. It
would make the harness reject `set_slider` on them (`AgentInput.cpp:78-82`
requires the inputable flag), it removes typed entry, and it does not block
the gamepad path.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`

## In scope

- The condition guarding the `mActiveSlider` and `miActiveSliderSection`
  assignments in `TweaksScreen::WrapperSlider`, in
  `TweaksScreen::RenderToggleBar` (Sun Angle), and after the `CurveWidget`
  call in `RenderLightingSection`.

## Out of scope

- The reset at `TweaksScreen.cpp:209-212` and every reader of
  `mActiveSlider`, including `UiScreens.cpp`.
- Slider flags, and making typed entry hide the screen like a drag.
- `CurveWidget` and `AgentInput`.
- The wave-count row's hide path; see `## Coordination`.
- The Tweaks documents, which already say "dragged".

## Acceptance criteria

- The diff shows the three set sites conditioned on the left button being
  down, and no other change.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness` with Tweaks shown, `set_slider` on a Tweaks float
  slider (for example `Water Scale` in the Water section) still succeeds and
  `describe_ui` reads back the new value. The supplied evidence records no
  harness setup.
- That the hide state is no longer entered without a drag is settled by code
  reading: the harness cannot hold a mouse button, and the half-hidden frames
  last only while the text input is open.

## Coordination

`Documents/Plans/Engine/AlignTweaksWaterAndPbrWithWrapperContracts.md` replaces
the wave-count row with a function that hides by reading `mActiveSlider`.
Neither Plan depends on the other. That function sets nothing, so this Plan's
three set sites stay the complete list in either order; whichever Plan lands
second applies its edit on top of the first.

One more Plan edits `TweaksScreen.cpp`. It does not depend on this Plan and
this Plan does not depend on it; whichever lands second re-derives its regions
by symbol from the landed file.

- `Documents/Plans/Engine/TweaksDefaultWindowUsesStaggeredX.md` edits
  `TweaksScreen::RenderSectionWindow` (`TweaksScreen.cpp:385-386`), a
  function that holds none of this Plan's set sites.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  runtime behavior of one developer screen and of what `RenderUiScreens` draws
  while it is used. Client-only, `kbDebugInput` builds only. No
  determinism/CRC, wire, serialization, replay, threading, allocation, or
  shader exposure.
- If the user wants typed entry to hide the screen like a drag instead, the
  reset would have to key on "no item active", which depends on Dear ImGui
  behavior not yet verified: whether `ImGui::IsAnyItemActive()` at the top of a
  frame still reports a slider in text input. That is a different change and
  outside this Plan.
