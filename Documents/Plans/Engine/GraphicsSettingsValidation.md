<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:58:44.014Z","dependsOn":[]} -->
# Graphics Settings Validation

## Context

`Engine/Source/Ui/GraphicsSettings.cpp` and `GraphicsSettings.h` (client-only)
carry three local leftovers, and the settings loaders restate a discrete
wrapper's allowed set outside the wrapper. All of it is pre-existing; none of
it changes what a settings file holds.

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
4. The allowed-set test restated outside the wrapper. A discrete `Wrapper`
   (`Engine/Source/Ui/WrapperBase.h`) holds only values from its `mAllowed`
   set; `GetIndex()` asserts on any other (`Engine/Source/Ui/AGENTS.md`
   `## Wrapper Contracts`). The settings loaders must therefore reject a file
   value outside that set before applying it. `Wrapper` offers `IsInRange`
   (`WrapperBase.h:100`) for float wrappers and no allowed-set query, so the
   loaders restate the test in three spellings across nine sites:
   - `std::ranges::contains(<wrapper>.mAllowed, static_cast<float>(value))`
     at `GraphicsSettings.cpp:140` (`gPresentMode`), `:145` (`gSampleCount`),
     and `GameSettings.cpp:69` (`gUiTheme`);
   - `IsValidGraphicsQualityLevel` (`GraphicsSettings.cpp:132-135`, a
     comparison against `GraphicsQualityLevel::kCount`) at `:195`, `:200`,
     `:205`, `:210`, and `:215`, for wrappers whose allowed sets are written
     separately in `GraphicsQualityWrappersBase.h:21-25`;
   - the literal `> 1` at `GraphicsSettings.cpp:220` for
     `gObjectShadowsBlurLevel`, whose allowed set `{0, 1}` is at
     `GraphicsQualityWrappersBase.h:27`.

   The second and third spellings do not read the wrapper at all. Adding a
   level to one of those wrappers without editing the validator either rejects
   every saved file or admits a value that trips `GetIndex()`'s assert at load.
5. An allowed value that makes the float comparison inexact. `gPresentMode`
   (`GraphicsSettingsWrappersBase.h:19`) lists
   `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` as a fourth allowed value. Its
   enumerator value is 1000361000 (Vulkan SDK 1.4.363.0 `vulkan_core.h`),
   which is not exactly representable as a `float` and rounds to
   `1000361024.0f`; the comment at `WrapperBase.h:187-190` records that. The
   compare at `GraphicsSettings.cpp:140` therefore also matches the
   neighbouring integers that round to the same float. The value is dead: the
   Graphics menu offers three modes (`GraphicsMenuScreen.cpp:137`),
   `SwapchainManager.cpp:236-251` honors only Mailbox and Immediate, falls back
   to FIFO, and resets the wrapper, and no other code writes `gPresentMode`
   (its other uses are reads in `ProfileScreens.cpp:148`, `Graphics.cpp:573`,
   and `SwapchainManager.cpp:311`). The saver stores
   `gPresentMode.Get<VkPresentModeKHR>()` (`GraphicsSettings.cpp:96`), so no
   program writes the value or its neighbours to a file; removing it is a
   dead-value removal, not load hardening. With it gone every discrete
   wrapper's allowed values are small integers, exact in `float`.

Originating gap: a deep analysis of `Engine/Source/Ui` — architecture-review
findings (returned names, unread result, duplicated validity test, unusable
allowed value) and a refactor finding (dead writes), each independently
verified against the lines above. Unmet criteria: each returned name is its
member's name; the four settings loaders share one signature; no statement
restores a value nothing changes; load validity of a discrete value is decided
by its wrapper's allowed set in one place; every `gPresentMode` allowed value
is selectable or honored.

## Design

User statement, recorded by the authoring agent from the user's own words
(2026-11-14): asked whether the private game project writes
`gWaterShapeDetail`, the user answered "Private game does not exist any code
yet". Both writes are therefore dead and step 6 removes both.

The author's recommendation:

1. Remove `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` from `gPresentMode`'s
   allowed list, and reduce the comment at `WrapperBase.h:187-190` to its
   first sentence (integer-backed values need exact float representation),
   dropping the sentences that exist only for that value.
2. Add one `const` member to `Wrapper` beside `IsInRange` that reports whether
   a value is in `mAllowed` (a name such as `IsAllowed`; the executing session
   picks it). Rationale: the test belongs with the set it reads, and nine
   current sites justify a helper under the DRY directive.
3. Rewrite `FindInvalidGraphicsSetting` in one pass so that each check and its
   returned name are settled together:
   - Replace the eight graphics sites (`:140`, `:145`, `:195`, `:200`, `:205`,
     `:210`, `:215`, `:220`) with the new member called on the field's own
     wrapper, and delete `IsValidGraphicsQualityLevel`.
   - Change the three returned strings to `"vkSampleCount"` (the block at
     `:145-148`, whose condition step 3 also rewrites),
     `"fMaximumAnisotropy"` (`:152`), and `"fMinimumSampleShading"` (`:157`).
   - Every other condition and every other returned string stays as it is.
     The function ends returning each member's own name.
4. Replace the `gUiTheme` site at `GameSettings.cpp:69` in
   `FindInvalidGameSetting` with the new member.
5. Declare and define `LoadGraphicsSettings` as `void` and delete
   `return bRead;`. Keep `bRead` as the local it is.
6. Delete lines `:262` and `:329`.

Behavior for today's sets is unchanged except that a file holding the removed
present mode is now rejected to defaults, which no program writes, and the
rejection warning names the real member for the three corrected fields.

## Critical files

- `Engine/Source/Ui/WrapperBase.h`
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.h`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/GraphicsSettings.h`
- `Engine/Source/Ui/GameSettings.cpp`

## In scope

- `WrapperBase.h`: one new `Wrapper` member beside `IsInRange`, and the comment
  above `Wrapper::mfStep` at `WrapperBase.h:187-190`.
- `GraphicsSettingsWrappersBase.h`: `gPresentMode`'s allowed-value list.
- `GraphicsSettings.cpp` and `GraphicsSettings.h`:
  - in `FindInvalidGraphicsSetting`, the eight validity tests named in
    `## Context` and the three string literals returned at `:147`, `:152`, and
    `:157`;
  - the function `IsValidGraphicsQualityLevel`;
  - the return type of `LoadGraphicsSettings` in both files, and its
    `return bRead;` statement;
  - the `gWaterShapeDetail` assignment in `LoadGraphicsSettings` and the one
    in `ResetGraphicsSettings`.
- `GameSettings.cpp`: the `gUiTheme` validity test in `FindInvalidGameSetting`.

## Out of scope

- The settings structs (`struct GraphicsSettings` and the others), their
  `static_assert`s, and their `kiVersion` values (`GraphicsSettings::kiVersion`
  included): no layout changes, so no version bump.
- The Boolean-byte tests (`> 1` on `uiWaterFoamEnabled`,
  `uiOrthographicProjection`, `uiBreakRepetition`), the `IsInRange` tests, and
  the `iLanguage` test.
- Every returned string in `FindInvalidGraphicsSetting` other than the three
  named in `## In scope`, and every string `FindInvalidGameSetting` returns.
- An assert tying `kpfObjectShadowsBlurMultipliers` or
  `ApplyObjectShadowsBlurLevel` to the wrapper: the wrapper already holds only
  allowed values, so it would check a state no writer produces.
- `Wrapper::GetIndex`, `Set`, `Reset`, and `SetIndex`.
- `SwapchainManager`, the Graphics menu, and adding a fourth present mode to
  either.
- The call at `Main.cpp:272`, which already ignores the result.
- `gWaterShapeDetail` itself, its readers, and the documents that describe it.

## Risk

Change Workflow tier: Tier 2 (the author's classification). Triggers: the
signature of a function declared in an engine header changes
(`LoadGraphicsSettings`), which Tier 1's "no public signature" excludes; and
scoped behavior of one subsystem's settings load, with a new public `Wrapper`
member. The checks stay inside the loaders at the existing file boundary with
the format and the trust unchanged, which `risk-tiers.md` places in Tier 2.

Invariants: the file format, the trust in the file, and every `kiVersion` are
untouched, so there is no serialization, determinism/CRC, wire, replay,
threading, allocation, or shader exposure; settings files are client
presentation state. `GraphicsSettings.cpp` is client-only code.

## Acceptance criteria

- The diff shows exactly: the removed allowed value, the shortened comment,
  one new `Wrapper` member, nine sites calling it,
  `IsValidGraphicsQualityLevel` gone, the three corrected returned names, the
  `void` loader, and the two removed `gWaterShapeDetail` assignments — and
  nothing else.
- The BrokenEngineSandbox client and server and ModelViewer build
  (`WrapperBase.h` compiles into all three; the client and ModelViewer compile
  `GraphicsSettings.cpp`).
- Through `/agent-harness`, a client started with the settings files an
  unchanged build saved loads them without a `rejected: invalid` warning in
  its log. The supplied evidence records no harness setup; a launch, a
  settings change saved with Back, and a relaunch serve.

## Notes

- Build exposure: `WrapperBase.h` is included widely, so the edit rebuilds many
  translation units in both builds.
- The comment at `WrapperBase.h:187-190` shows the loadable fourth present
  mode was known and left in place. The recommendation to remove it rests on
  YAGNI: no UI offers it and the swapchain never requests it.
- The private game project has no code yet, so `Main.cpp:272` is the only
  caller of `LoadGraphicsSettings`.
