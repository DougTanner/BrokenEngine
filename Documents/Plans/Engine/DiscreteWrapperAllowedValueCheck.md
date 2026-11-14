<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:58:44.014Z","dependsOn":[]} -->
# Check Discrete Settings Against the Wrapper's Allowed Set in One Place

## Context

A discrete `Wrapper` (`Engine/Source/Ui/WrapperBase.h`) holds only values from
its `mAllowed` set; `GetIndex()` asserts on any other
(`Engine/Source/Ui/AGENTS.md` `## Wrapper Contracts`). The settings loaders must
therefore reject a file value outside that set before applying it. `Wrapper`
offers `IsInRange` (`WrapperBase.h:100`) for float wrappers and no
allowed-set query, so the loaders restate the test outside the wrapper, in
three spellings across nine sites:

- `std::ranges::contains(<wrapper>.mAllowed, static_cast<float>(value))` at
  `GraphicsSettings.cpp:140` (`gPresentMode`), `:145` (`gSampleCount`), and
  `GameSettings.cpp:69` (`gUiTheme`);
- `IsValidGraphicsQualityLevel` (`GraphicsSettings.cpp:132-135`, a comparison
  against `GraphicsQualityLevel::kCount`) at `:195`, `:200`, `:205`, `:210`,
  and `:215`, for wrappers whose allowed sets are written separately in
  `GraphicsQualityWrappersBase.h:21-25`;
- the literal `> 1` at `GraphicsSettings.cpp:220` for
  `gObjectShadowsBlurLevel`, whose allowed set `{0, 1}` is at
  `GraphicsQualityWrappersBase.h:27`.

The second and third spellings do not read the wrapper at all. Adding a level
to one of those wrappers without editing the validator either rejects every
saved file or admits a value that trips `GetIndex()`'s assert at load.

One allowed value makes the float comparison inexact. `gPresentMode`
(`GraphicsSettingsWrappersBase.h:19`) lists
`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` as a fourth allowed value. Its
enumerator value is 1000361000 (Vulkan SDK 1.4.363.0 `vulkan_core.h`), which
is not exactly representable as a `float` and rounds to `1000361024.0f`; the
comment at `WrapperBase.h:187-190` records that. The compare at
`GraphicsSettings.cpp:140` therefore also matches the neighbouring integers
that round to the same float. The value is dead: the Graphics menu offers three
modes (`GraphicsMenuScreen.cpp:137`), `SwapchainManager.cpp:236-251` honors
only Mailbox and Immediate, falls back to FIFO, and resets the wrapper, and no
other code writes `gPresentMode` (its other uses are reads in
`ProfileScreens.cpp:148`, `Graphics.cpp:573`, and `SwapchainManager.cpp:311`).
The saver stores `gPresentMode.Get<VkPresentModeKHR>()`
(`GraphicsSettings.cpp:96`), so no program writes the value or its neighbours
to a file; removing it is a dead-value removal, not load hardening. With it
gone every discrete wrapper's allowed values are small integers, exact in
`float`.

Originating gap: a deep analysis of `Engine/Source/Ui` — two
architecture-review findings (duplicated validity test; unusable allowed
value), each independently verified. Unmet criteria: load validity of a
discrete value is decided by its wrapper's allowed set in one place; every
`gPresentMode` allowed value is selectable or honored.

## Design

The author's recommendation:

1. Remove `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` from `gPresentMode`'s
   allowed list, and reduce the comment at `WrapperBase.h:187-190` to its
   first sentence (integer-backed values need exact float representation),
   dropping the sentences that exist only for that value.
2. Add one `const` member to `Wrapper` beside `IsInRange` that reports whether
   a value is in `mAllowed` (a name such as `IsAllowed`; the executing session
   picks it). Rationale: the test belongs with the set it reads, and nine
   current sites justify a helper under the DRY directive.
3. Replace all nine sites with that member called on the field's own wrapper,
   and delete `IsValidGraphicsQualityLevel`.

Behavior for today's sets is unchanged except that a file holding the removed
present mode is now rejected to defaults, which no program writes.

## Critical files

- `Engine/Source/Ui/WrapperBase.h`
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.h`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/GameSettings.cpp`

## In scope

- `gPresentMode`'s allowed-value list.
- The comment above `Wrapper::mfStep` at `WrapperBase.h:187-190`.
- One new `Wrapper` member beside `IsInRange`.
- The nine validity tests named in `## Context` in
  `FindInvalidGraphicsSetting` and `FindInvalidGameSetting`, and the function
  `IsValidGraphicsQualityLevel`.

## Out of scope

- The settings structs, their `static_assert`s, and their `kiVersion` values:
  no layout changes, so no version bump.
- The Boolean-byte tests (`> 1` on `uiWaterFoamEnabled`,
  `uiOrthographicProjection`, `uiBreakRepetition`), the `IsInRange` tests, and
  the `iLanguage` test.
- An assert tying `kpfObjectShadowsBlurMultipliers` or
  `ApplyObjectShadowsBlurLevel` to the wrapper: the wrapper already holds only
  allowed values, so it would check a state no writer produces.
- `Wrapper::GetIndex`, `Set`, `Reset`, and `SetIndex`.
- `SwapchainManager`, the Graphics menu, and adding a fourth present mode to
  either.
- The strings `FindInvalidGraphicsSetting` returns; see `## Coordination`.

## Acceptance criteria

- The diff shows the removed allowed value, the shortened comment, one new
  `Wrapper` member, nine sites calling it, and `IsValidGraphicsQualityLevel`
  gone.
- The BrokenEngineSandbox client and server and ModelViewer build
  (`WrapperBase.h` compiles into all three).
- Through `/agent-harness`, a client started with the settings files an
  unchanged build saved loads them without a `rejected: invalid` warning in
  its log. The supplied evidence records no harness setup; a launch, a
  settings change saved with Back, and a relaunch serve.

## Coordination

`Documents/Plans/Engine/CleanUpGraphicsSettingsLoader.md` corrects three
strings `FindInvalidGraphicsSetting` returns, one of them in the block at
`:145-148` this Plan rewrites. Neither Plan depends on the other. Whichever
lands second applies its edit on top of the first; this Plan changes only the
conditions, never a returned string.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  behavior of one subsystem's settings load, with a new public `Wrapper`
  member. The checks stay inside the loaders at the existing file boundary
  with the format and the trust unchanged, which `risk-tiers.md` places in
  Tier 2. No determinism/CRC, wire, replay, threading, allocation, or shader
  exposure; settings files are client presentation state.
- Build exposure: `WrapperBase.h` is included widely, so the edit rebuilds many
  translation units in both builds.
- The comment at `WrapperBase.h:187-190` shows the loadable fourth value was
  known and left in place. The recommendation to remove it rests on YAGNI: no
  UI offers it and the swapchain never requests it.
