<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:16:29.772Z","dependsOn":[]} -->
# Turn the engine tweak section renderers into free functions so TweaksScreenBase is defined in one .cpp

## Context

`Documents/Plans/Game/OneCppPerClass.md` adds style-guide rule 67: a type declared with the `class` keyword keeps every member definition that is not in its header in one `.cpp` file, while `struct` types and free functions are exempt. Its `## Out of scope` leaves engine `TweaksScreenBase` to a separate Plan; this is that Plan. User decision: the class is brought under the rule by turning its ten engine section renderers into free functions (Option A), not by merging files. The merged file would be about 16,800 bt-token-v1, far over the `/reduce-file` `.cpp` threshold of 10,000.

Source inspection at `81cf299f`:

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h:77-90` declares ten public non-virtual members `RenderPbrSection`, `RenderTerrainSection`, `RenderWaterSection`, `RenderLightingSection`, `RenderShadowSection`, `RenderSunMoonSection`, `RenderMiscSection`, `RenderSoundSection`, `RenderSmokeSection`, and `RenderWindSection`. The five virtual game hooks are interleaved with them (`RenderLightingEffectsVisibleTab`, `RenderLightingEffectsLightingTab`, `RenderSoundEffects`, `RenderSmokeDepositsTab`, `RenderWindDepositsTab`).
- Each member is defined in its own file: `TweaksScreenPbr.cpp:51`, `TweaksScreenTerrain.cpp:38`, `TweaksScreenWater.cpp:121`, `TweaksScreenLighting.cpp:114`, `TweaksScreenShadow.cpp:40`, `TweaksScreenSunMoon.cpp:48`, `TweaksScreenMisc.cpp:16`, `TweaksScreenSound.cpp:38`, `TweaksScreenSmoke.cpp:46`, and `TweaksScreenWind.cpp:36`. These are the only `TweaksScreenBase::` definitions outside `TweaksScreenBase.cpp`. Besides the member, each file holds only its `const TweaksSliderMapRegistrar g<Section>Registrar`.
- `TweakSectionDesc::pfnRender` is `void (*)(TweaksScreenBase& rScreen)` (`TweaksScreenBase.h:33`). `RegisterEngineTweakSections()` (`TweaksScreenBase.cpp:56-65`) wraps each member in a capture-free lambda, `[](TweaksScreenBase& rScreen) { rScreen.Render<Section>Section(); }`. `RenderSectionWindow` and `RunSliderAuditFrame` invoke the pointer as `GetSection(i).pfnRender(*this)` (`TweaksScreenBase.cpp:427`, `:471`).
- The renderer bodies hold no state. They use only screen members, which are all public: `WrapperSlider` (334 uses), `WrapperSeparatorText` (61), `BeginSubtab` (16), `ChevronIndexSelector` (3), `RenderWaveCountRadioButtons` (2), one call to each of the five virtual hooks, and, in `TweaksScreenLighting.cpp:217-218` only, the data members `mActiveSlider` and `miActiveSliderSection`. Everything else they use is wrapper globals, `giTweakSection*`, ImGui, and `CurveWidget`.
- Nothing outside the lambdas calls the ten members. `git grep` finds no other caller in `Engine/` or `Projects/`. The game `TweaksScreen` (`Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`) overrides only the five virtual hooks.
- `Engine/Source/Ui/MenuUtils.h:56` declares a free function `engine::WrapperSlider(std::string_view, engine::Wrapper*, std::string_view)`. Inside a free function, an unqualified `WrapperSlider("...", iSection, ...)` would find that overload instead of the member, and an `int64_t` section index does not convert to `Wrapper*`. So any call site the conversion misses fails to compile instead of silently changing meaning.

## Design

The author recommends the following.

1. Header (`TweaksScreenBase.h`). Delete the ten non-virtual `Render<Section>Section()` member declarations. Keep the five virtual hooks where they are. After the `giTweakSection*` globals and before `RegisterEngineTweakSections()`, declare ten free functions in `namespace engine`, in the same order as the deleted members:

   ```cpp
   void RenderPbrSection(TweaksScreenBase& rScreen);
   ...
   void RenderWindSection(TweaksScreenBase& rScreen);
   ```

   Each signature matches `TweakSectionDesc::pfnRender` exactly. The names do not change.

2. Definitions. In each of the ten section files, change `void TweaksScreenBase::Render<Section>Section()` to `void Render<Section>Section(TweaksScreenBase& rScreen)`. Keep the function in its current file and position, next to its registrar. Qualify every use of a screen member in the body with `rScreen.`: the five widget helpers, the five hook calls (which still dispatch virtually through the reference), and `rScreen.mActiveSlider` / `rScreen.miActiveSliderSection` in Lighting. That is about 425 sites. Change nothing else in the bodies: no reordering, no reformatting of unrelated lines, and no registrar edits. The functions are used from another translation unit, so they take no `static` (rule 66).

3. Registration (`TweaksScreenBase.cpp:56-65`). Replace each lambda with the function name, for example `.pfnRender = RenderPbrSection`. Keep display names, stable keys, and registration order unchanged so the layout CRC over stable keys stays the same.

4. Access. Every member the functions use is already public, so no access specifier, `friend`, or accessor changes are needed.

Recorded decision (user): the free functions take `TweaksScreenBase&`, as `pfnRender` already does. `.agents/references/cpp-conventions.md` discourages passing a game-instantiated `*Base` as a parameter. Here the existing `TweakSectionDesc::pfnRender` signature already passes the screen, and the screen has no `gp*` global of its own (it is reached as `gpImGuiManager->mpTweaksScreen`). This Plan keeps that precedent and adds no global.

Change Workflow tier: Tier 2. Trigger: changes publicly declared signatures in `TweaksScreenBase.h`, within one subsystem (the client tweaks UI), with no behavior change. It is not Tier 1 because the public header declarations change. Determinism/CRC, wire, serialization (`TweakSectionState` and its 296-byte assertion, the layout CRC, the settings version), `.pack`, replay, threading, allocation, and shaders are unaffected. All files are `BT_CLIENT`-only and already members of the client project, so project membership does not change.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp` (`RegisterEngineTweakSections()` only)
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenPbr.cpp`, `TweaksScreenTerrain.cpp`, `TweaksScreenWater.cpp`, `TweaksScreenLighting.cpp`, `TweaksScreenShadow.cpp`, `TweaksScreenSunMoon.cpp`, `TweaksScreenMisc.cpp`, `TweaksScreenSound.cpp`, `TweaksScreenSmoke.cpp`, `TweaksScreenWind.cpp`

## In scope

- Deleting the ten `Render<Section>Section()` member declarations from `class TweaksScreenBase`, and declaring the ten same-named free functions taking `TweaksScreenBase& rScreen` in `TweaksScreenBase.h`.
- In each section file, the signature line of its renderer and the `rScreen.` qualification of screen-member uses inside that function's body.
- The ten `.pfnRender` initializers in `RegisterEngineTweakSections()`.

## Out of scope

- The five virtual hooks, their declarations, and every game-side file, including `game::TweaksScreen` and its registrations and lambdas.
- `TweakSectionDesc`, the `pfnRender` type, `RegisterSection`, `RenderSectionWindow`, `RunSliderAuditFrame`, and every other `TweaksScreenBase` member and data member.
- Registrars, slider labels, map keys, slider order, comments in the section files (including `TweaksScreenSmoke.cpp:36`, which `OneCppPerClass.md` edits), and `TweaksSliderMap.h`/`.cpp`.
- Adding a `gp*` global for the screen, accessors, `friend` declarations, or access-specifier changes.
- Merging, splitting, renaming, or moving files; project membership; include order; any other style cleanup.
- `Documents/C++StyleGuide.txt` and skill text (rule 67 belongs to `OneCppPerClass.md`).

## Acceptance criteria

- `git grep -lE 'TweaksScreenBase::' -- 'Engine/*.cpp'` returns only `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp`.
- `class TweaksScreenBase` declares none of the ten `Render<Section>Section` names. `TweaksScreenBase.h` declares each as a free function taking `TweaksScreenBase& rScreen`.
- `RegisterEngineTweakSections()` contains no lambda (`git grep -nF '.pfnRender = [](' -- Engine/` returns nothing). Display names, stable keys, and order are byte-identical to before.
- The diff of each section file changes only its renderer's signature line and adds `rScreen.` to member uses.
- `/compile` builds the BrokenEngineSandbox client `Debug|x64` cleanly with Shared runtime data. The server build is not required because every touched file is `BT_CLIENT`-only.
- No live verification is needed: the same function bodies run through the same pointer slots, virtual hooks still dispatch through the reference, and the compiler rejects any unqualified helper call the conversion misses (see `## Context`).

## Coordination

No dependencies or mandatory coordination constraints. Related Plans, valid in either landing order:

- `Documents/Plans/Game/OneCppPerClass.md` adds rule 67, which this Plan satisfies for `TweaksScreenBase`, and edits the comment at `TweaksScreenSmoke.cpp:36` and the registrar comment at `TweaksSliderMap.h:16`. This Plan changes neither line, and its `TweaksScreenSmoke.cpp` edit is the renderer at line 46. Whichever lands second applies its edits by content, not by line number.
- `Documents/Plans/Engine/TweakSubtabArraySnapshots.md` changes the `mActiveSubtab`/`mPreAuditSubtab` declarations in `TweaksScreenBase.h` and their copies in `TweaksScreenBase.cpp`. Those lines are separate from the ones this Plan edits, and the renderers do not use either member.

## Notes

- Free functions are exempt under rule 67, so the ten section files stay as they are. `TweaksScreenBase.cpp` then holds every `TweaksScreenBase` member definition, and no file is resized.
- After the change, `TweaksScreen/AGENTS.md` stays accurate ("sub-tabs inside an engine section use the base extension hooks"). `/update-claude-docs` should confirm this rather than add text.
