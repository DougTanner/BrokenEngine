<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:42.194Z","dependsOn":[]} -->
# Make Tweaks slider keys match their labels

## Context

Observed symptom, measured at commit 5c2e6a8d: 479 `WrapperSlider` calls under
`Engine/Source` and `Projects/BrokenEngineSandbox/Source` pass a fourth string
argument: in 298 a `mapKey` that differs from the label, and in 181 a repeat of
the label. A differing slider carries two strings plus its
`TweaksSliderMapRegistrar` key, `TweaksScreen::WrapperSlider`
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:146`) appends a hidden
`##key` to its ImGui ID, and the caller has to spell out the multiplier to reach
the fourth parameter.

It came from commits cfd6116e, 13e4ae19, and 5c2e6a8d: each label became the
wrapper identifier's words, as the `Engine/Source/Ui/AGENTS.md` label rule
requires, and the old string moved to `mapKey` so the registrars stayed
untouched. The labels are wanted; keeping the old keys is the cost. A sample of
`TweaksScreenShadow.cpp`, `TweaksScreenSmoke.cpp`, and `TweaksScreenWater.cpp`
found the labels map correctly to their wrappers.

Whether anything outside the screen depends on the old keys is not yet settled.
What authoring read:

- Saved settings: `TweaksSettings.bin` is gated on section stable keys
  (`Screens/TweaksScreen/AGENTS.md`); `Engine/Source/Ui/TweaksSettings.cpp` shows
  no slider key.
- Harness: `get_wrapper` looks up `TweaksSliderMap::Get()` by key
  (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:932`), so its callers name
  old keys. `set_slider` and `click` go through
  `AgentUiRegistry::ResolveLabel`, which tries the full label first and then the
  part before `##`.
- `Screens/TweaksScreen/AGENTS.md` says a label may differ from its key to avoid
  ImGui ID collisions, so some `mapKey` arguments may be load-bearing.

## Design

First the executing session confirms the dependency picture by reading every
consumer `## In scope` lists. This is a cleanup survey, not a defect, so it needs
no `/external-diagnose-bug`.

Then it makes the smallest change in the region: rename each registrar key to its
slider's label, delete the now-equal fourth argument, and delete a multiplier
argument only where it repeats the declared `fWidthMultiplier` default. It keeps
`mapKey` only at a call whose label really collides, and leaves
`WrapperSlider`'s signature alone.

If a saved file or other persisted data turns out to key on slider keys, or the
fix needs anything outside the region, it stops and surfaces that for re-planning.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md`
- `.agents/skills/agent-harness/references/command-reference.md`

## In scope

- Investigation, consumers to read: `TweaksSettings`, `AgentCommandsClientGeneric`
  `get_wrapper`, `AgentUiRegistry::ResolveLabel`, the `mAuditTouched` and
  `mAuditMissed` audit in `TweaksScreen::WrapperSlider`, and any label repeated
  inside one ImGui window.
- Fix region: the `TweaksSliderMapRegistrar` entries and `WrapperSlider` calls in
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen*.cpp` and
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen*.cpp`.
- Documentation naming a renamed key: `Screens/TweaksScreen/AGENTS.md`, the
  `get_wrapper` text of `command-reference.md`, and other harness documents.
- Live Plan text naming a renamed key, such as the `get_wrapper` keys in the
  acceptance section of `Documents/Plans/Engine/InlineUiWrapperInitialization.md`.

## Out of scope

- Label text, wrappers, their values, ranges, and order.
- The section stable keys and `TweaksSettings` layout or version.
- `TweaksScreen::WrapperSlider`'s signature and the `mapKey` parameter itself.
- Which section a slider appears in; removing a duplicated slider belongs to
  `Documents/Plans/Engine/EngineExportRenderAndViewerCleanups.md`.

## Risk triggers and invariants

- Tier 2. Trigger: one subsystem's runtime and tool behavior, the Tweaks slider
  keys and the agent `get_wrapper` keys. A persisted key found by the
  investigation would reach serialization, Tier 3, and returns for re-planning.
- Client only and debug-facing: no Frame, CRC, wire, replay, or `.pack` exposure.
- Registrar keys stay globally unique: `TweaksSliderMapRegistrar` asserts on a
  duplicate at static initialization, so a key collision fails at startup.
- The registry stays written once at startup and immutable while rendering.

## Acceptance criteria

- The handoff records each consumer's result from the investigation.
- The recorded symptom no longer reproduces: a `WrapperSlider` call passes
  `mapKey` only where its label collides, and each such call is listed.
- `/compile` builds the BrokenEngineSandbox client and server, and ModelViewer.
- A live `/agent-harness` run: the Tweaks first-open audit logs no missing or
  orphaned registration, `get_wrapper` answers for a sample of renamed keys from
  each section, and `set_slider` still reaches a sample slider by label. The
  supplied evidence records no harness run or setup; the audit read source only.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
