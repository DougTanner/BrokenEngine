<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-04T14:55:13.272Z","dependsOn":[]} -->
# Turn the game tweak section renderers into free functions and register the sub-tab hooks so game TweaksScreen is defined in one .cpp

## Context

`Documents/Plans/Game/OneCppPerClass.md` adds style-guide rule 67: a type declared with the `class` keyword keeps every member definition that is not in its header in one `.cpp` file, while `struct` types and free functions are exempt. User decision: game `TweaksScreen` is out of that Plan's realized scope and is brought under the rule here by the free-function pattern the engine already applied to `TweaksScreenBase` (landed in `e2fda7fa`, "Turn the engine tweak section renderers into free functions"), not by merging files. For the five sub-tab hooks the user stated: "Need a way for the game:: code to 'register' the new screens, but should be implemented as close as possible to what the engine:: ones will be".

Source inspection at `e2fda7fa`:

- `class game::TweaksScreen` (`Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h:10-26`) declares `Render()`, two plain members `RenderHexShieldSection` and `RenderParticlesSection` (`:16-18`, with a comment line), and five overrides of the `TweaksScreenBase` virtual hooks (`:20-25`, with a comment line): `RenderSmokeDepositsTab`, `RenderWindDepositsTab`, `RenderLightingEffectsVisibleTab`, `RenderLightingEffectsLightingTab`, and `RenderSoundEffects`.
- `TweaksScreen.cpp` defines only `Render()` and `RegisterGameTweakSections()`. The other seven members are each defined in their own file, next to that file's `TweaksSliderMapRegistrar`: `TweaksScreenHexShield.cpp:29`, `TweaksScreenParticles.cpp:60`, `TweaksScreenSmokeDeposits.cpp:46`, `TweaksScreenWindDeposits.cpp:33`, `TweaksScreenLightingEffectsVisible.cpp:54`, `TweaksScreenLightingEffectsLighting.cpp:57`, and `TweaksScreenSoundEffects.cpp:44`.
- `RegisterGameTweakSections()` (`TweaksScreen.cpp:23-36`) registers HexShield and Particles through `engine::TweaksScreenBase::RegisterSection` with capture-free lambdas that downcast the screen to `TweaksScreen&` and call the member; the comment at `:25` justifies that downcast.
- `engine::TweaksScreenBase` still declares the five hooks as public virtual members with empty bodies (`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h:77-81`). The engine calls each exactly once, from an engine section free function, through its `rScreen` parameter: `TweaksScreenLighting.cpp:299` and `:304` (in `RenderLightingSection`), `TweaksScreenSound.cpp:52` (`RenderSoundSection`), `TweaksScreenSmoke.cpp:97` (`RenderSmokeSection`), and `TweaksScreenWind.cpp:83` (`RenderWindSection`). Nothing else calls the hooks or the two game section members.
- The landed engine pattern (`TweaksScreenBase.h:123-132`, `TweaksScreenBase.cpp:54-66`): each engine section renderer is a free function `void Render<Section>Section(TweaksScreenBase& rScreen)` in `namespace engine`, declared after the `giTweakSection*` globals and before `RegisterEngineTweakSections()`, defined in its own file next to its registrar, and named directly in its registration as `.pfnRender = Render<Section>Section`.
- The landed engine registration shape: `struct TweakSectionDesc` holding `pfnRender` of type `void (*)(TweaksScreenBase& rScreen)` (`TweaksScreenBase.h:26-34`); `static RegisterSection` (`:64`) writing into the file-static table `sSectionDescs` (`TweaksScreenBase.cpp:12`) with startup ASSERTs (`:30-37`); `static GetSection` (`TweaksScreenBase.h:66`, `TweaksScreenBase.cpp:44-47`) reading it back; and the engine invoking `GetSection(i).pfnRender(*this)` (`TweaksScreenBase.cpp:427`, `:471`). `game::RegisterGameTweakSections()` runs from `Engine/Source/Main.cpp:309`, right after `RegisterEngineTweakSections()` (`:308`) and before the Graphics constructor builds `ImGuiManager`, which owns the one screen as `std::unique_ptr<game::TweaksScreen>` (`ImGuiManager.h:62`, `ImGuiManager.cpp:171`).
- The seven game bodies hold no state and use only three screen members, all public base members: `WrapperSlider` (about 185 uses), `WrapperSeparatorText` (about 55), and `BeginSubtab` (3, in Particles). Everything else they use is wrapper globals, `giTweakSection*`, and ImGui.
- Inside a free function in `namespace game`, an unqualified `WrapperSlider`, `WrapperSeparatorText`, or `BeginSubtab` call does not find the base member, and `Projects/` has no `using namespace engine`, so any site the conversion misses fails to compile instead of changing meaning.

## Design

The author recommends the following.

1. Engine registration (`TweaksScreenBase.h`, `TweaksScreenBase.cpp`). Delete the five virtual hook declarations (`TweaksScreenBase.h:77-81`) and the blank line that follows them from `class TweaksScreenBase`. Directly after `struct TweakSectionDesc`, add a `struct TweakExtensionHooks` with five members of the `pfnRender` type, `void (*)(TweaksScreenBase& rScreen)`, each `= nullptr`, named after the hooks they replace and in the current header order: `pfnRenderLightingEffectsVisibleTab`, `pfnRenderLightingEffectsLightingTab`, `pfnRenderSoundEffects`, `pfnRenderSmokeDepositsTab`, `pfnRenderWindDepositsTab`. Directly after the `GetSection` declaration in the class, declare `static void RegisterExtensionHooks(const TweakExtensionHooks& rHooks);` and `static const TweakExtensionHooks& GetExtensionHooks();`; the startup-only comment above `RegisterSection` already covers them. In `TweaksScreenBase.cpp`, add a file-static `static TweakExtensionHooks sExtensionHooks {};` directly after `siSectionCount`, and define the two functions directly after `GetSection`. `RegisterExtensionHooks` ASSERTs that nothing was registered before (`sExtensionHooks.pfnRenderLightingEffectsVisibleTab == nullptr`, mirroring `RegisterSection`'s double-registration ASSERT), ASSERTs each of the five incoming pointers is non-null, and copies the struct. `GetExtensionHooks` ASSERTs that registration happened (`sExtensionHooks.pfnRenderLightingEffectsVisibleTab != nullptr`; registration stores all five or none) and returns the struct. Rationale: this is the landed section-registration shape — descriptor struct, static register, file-static storage, static get — applied to the hooks, with no new global, accessor to screen state, `friend`, or access change. Per the user, an unregistered slot is an ASSERT, not a handled case.

2. Engine call sites. Replace each of the five `rScreen.<Hook>();` calls with a call through the registered pointer that passes the same reference, for example `TweaksScreenBase::GetExtensionHooks().pfnRenderSmokeDepositsTab(rScreen);`, mirroring `GetSection(i).pfnRender(*this)`. Change no other line in those files.

3. Game header (`TweaksScreen.h`). Delete the seven render member declarations, their two comment lines, and the blank lines between them (`:15-25`) from `class TweaksScreen`, leaving `Render()`. After the `giTweakSection*` globals and before the comment above `RegisterGameTweakSections()`, declare seven free functions in `namespace game`, in the same order as the deleted members, each `void <Name>(engine::TweaksScreenBase& rScreen);` with its name unchanged, as the engine declares its ten. They are used from `TweaksScreen.cpp`, another translation unit, so they take no `static` (rule 66).

4. Game definitions. In each of the seven section files, change `void TweaksScreen::<Name>()` to `void <Name>(engine::TweaksScreenBase& rScreen)`, keeping the function in its current file and position next to its registrar. Qualify every use of a screen member in the body with `rScreen.`. Change nothing else: no reordering, no unrelated reformatting, no registrar edits.

5. Game registration (`RegisterGameTweakSections()`). Replace the two downcasting lambdas with `.pfnRender = RenderHexShieldSection` and `.pfnRender = RenderParticlesSection`, as `RegisterEngineTweakSections()` names its functions, and delete the downcast comment at `TweaksScreen.cpp:25`, which no longer applies. Keep display names, stable keys, and order unchanged so the layout CRC stays the same. After the two `RegisterSection` calls, call `engine::TweaksScreenBase::RegisterExtensionHooks` with the five game functions as designated initializers in the struct's member order.

Recorded decision (user): the free functions take `engine::TweaksScreenBase&`, the existing `pfnRender` signature, as the landed engine renderers do. `.agents/references/cpp-conventions.md` discourages passing a game-instantiated `*Base` as a parameter; the engine kept the `pfnRender` precedent because the screen has no `gp*` global of its own, and this Plan adds none.

Engine/game contract: `Engine/Source/AGENTS.md` `## Hub Conventions` lets engine code consume hooks the game is required to provide. The `TweakExtensionHooks` member names repeat the game sub-tab names that the virtual hooks already carry in the engine header, so this adds no new game concept to the engine.

Change Workflow tier: Tier 2. Trigger: changes publicly declared signatures and the engine-to-game hook mechanism in `TweaksScreenBase.h` and `TweaksScreen.h`, within one subsystem (the client tweaks UI), with no behavior change. It is not Tier 1 because the public header declarations change. Determinism/CRC, wire, serialization (`TweakSectionState` and its 296-byte assertion, the layout CRC, the settings version), `.pack`, replay, threading, allocation, and shaders are unaffected. Registration stays startup-only and single-threaded, before the first UI frame. All touched files are `BT_CLIENT`-only and already members of the client project, so project membership does not change.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`, `TweaksScreenSmoke.cpp`, `TweaksScreenSound.cpp`, `TweaksScreenWind.cpp` (the hook call lines only)
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp` (`RegisterGameTweakSections()` only)
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp`, `TweaksScreenParticles.cpp`, `TweaksScreenSmokeDeposits.cpp`, `TweaksScreenWindDeposits.cpp`, `TweaksScreenLightingEffectsVisible.cpp`, `TweaksScreenLightingEffectsLighting.cpp`, `TweaksScreenSoundEffects.cpp`

## In scope

- `TweaksScreenBase.h`: deleting the five virtual hook declarations; adding `struct TweakExtensionHooks` after `TweakSectionDesc`; declaring `RegisterExtensionHooks` and `GetExtensionHooks` after `GetSection`.
- `TweaksScreenBase.cpp`: the file-static `sExtensionHooks` after `siSectionCount`, and the definitions of `RegisterExtensionHooks` and `GetExtensionHooks` after `GetSection`.
- The five hook call lines in `RenderLightingSection`, `RenderSmokeSection`, `RenderSoundSection`, and `RenderWindSection`.
- `TweaksScreen.h`: deleting the seven render member declarations and their two comment lines from `class TweaksScreen`, and declaring the seven same-named free functions.
- In each of the seven game section files, the signature line of its renderer and the `rScreen.` qualification of screen-member uses inside that function's body.
- `RegisterGameTweakSections()`: the two `.pfnRender` initializers, the downcast comment, and the added `RegisterExtensionHooks` call.
- Any comment that, when this Plan is implemented, names one of the seven game renderers in its `TweaksScreen::` qualified form (none exists at `e2fda7fa`; see `## Coordination`): dropping the `TweaksScreen::` qualifier only.

## Out of scope

- `TweakSectionDesc`, the `pfnRender` type, `RegisterSection`, `SectionCount`, `GetSection`, `AllSectionFlags`, `RegisterEngineTweakSections()`, `RenderSectionWindow`, `RunSliderAuditFrame`, the virtual destructor, and every other `TweaksScreenBase` member and data member.
- `game::TweaksScreen::Render()`, `ImGuiManager`, and `Main.cpp`.
- Every engine section renderer line other than the five hook calls.
- Registrars, slider labels, map keys, slider order, display names, stable keys, section order, `TweaksSliderMap.h`/`.cpp`, and other comments, including the three `TweaksScreenSmokeDeposits` comments and `ParticleWrappers.h:10`, which name files that still exist.
- Adding a `gp*` global for the screen, accessors to screen state, `friend` declarations, or access-specifier changes.
- Merging, splitting, renaming, or moving files; project membership; include order; any other style cleanup.
- `Documents/C++StyleGuide.txt` and skill text.

## Acceptance criteria

- `git grep -nE 'TweaksScreen::' -- Projects/ Engine/` lists only the `TweaksScreen::Render()` definition in `TweaksScreen.cpp`, and `class TweaksScreen` declares only `Render()`.
- `git grep -nE 'virtual void Render' -- Engine/Source/Ui/Screens/TweaksScreen/` returns nothing.
- `git grep -nF 'static_cast<TweaksScreen&>' -- Projects/` returns nothing, and `RegisterGameTweakSections()` keeps its display names, stable keys, and order byte-identical.
- Each of the five engine hook call sites calls through `GetExtensionHooks()`, `git grep -c 'GetExtensionHooks()\.' -- 'Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen[LSW]*.cpp'` totals 5, and `RegisterGameTweakSections()` registers all five game functions.
- The diff of each game section file changes only its renderer's signature line and adds `rScreen.` to member uses.
- `/compile` builds the BrokenEngineSandbox client `Debug|x64` cleanly with Shared runtime data. The server build is not required because every touched file is `BT_CLIENT`-only.
- A Debug client launched through `/agent-harness` reaches the main menu, which runs registration and its ASSERTs at startup. No further live verification is needed: the same bodies run through the same pointer slots or through registered pointers with the same signature, and the compiler rejects any unqualified helper call the conversion misses (see `## Context`).

## Coordination

No dependencies or mandatory coordination constraints. The engine prerequisite, `Documents/Plans/Engine/TweakSectionRenderFreeFunctions.md`, landed in `e2fda7fa` and is no longer in the tree. Related Plans, valid in either landing order:

- `Documents/Plans/Game/OneCppPerClass.md` adds rule 67, which this Plan satisfies for game `TweaksScreen`; once that Plan lands, rule 67 is in `Documents/C++StyleGuide.txt`. By user decision `TweaksScreen` is outside its realized scope. If its text still carries the `TweaksScreen` merge step, or its stale-comment edits naming `TweaksScreen::RenderParticlesSection()` and `TweaksScreen::RenderSmokeDepositsTab()`, those conflict with this Plan's free-function design, and the user's decision moving `TweaksScreen` here is the authority. Any such qualified name that did land is handled by this Plan's `## In scope` comment item.
- `Documents/Plans/Game/SplitCppCorrespondingHeaderOrder.md` reorders the include blocks of the seven game section files, and `Documents/Plans/Engine/TweakSubtabArraySnapshots.md` changes the `mActiveSubtab`/`mPreAuditSubtab` declarations in `TweaksScreenBase.h` and their copies in `TweaksScreenBase.cpp`. Neither touches the lines this Plan edits, but both shift line numbers, so whichever lands second applies its edits by content, not by line number.

## Notes

- Free functions are exempt under rule 67, so the seven game section files stay as they are. `TweaksScreen.cpp` then holds every `TweaksScreen` member definition, and no file is resized.
- `/update-claude-docs` should check `Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` ("sub-tabs inside an engine section use the base extension hooks", and the See Also entry "Extension hooks") and `Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` ("implement extension hooks for sub-tabs"): the hooks become functions the game registers instead of virtual overrides.
