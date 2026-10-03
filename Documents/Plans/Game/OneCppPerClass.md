<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:09:46.968Z","dependsOn":[]} -->
# Keep each class's member definitions in one .cpp file, and merge three split classes

## Context

User decision (D1): add a numbered rule to `Documents/C++StyleGuide.txt` saying a type declared with the `class` keyword keeps all its member definitions in one `.cpp` file. Types declared `struct`, including frame Collections, and free functions are exempt. The three split classes whose merged file fits under the `/reduce-file` `.cpp` threshold (10,000 bt-token-v1, `.agents/skills/reduce-file/references/worker.md` step 2) merge in this Plan. The over-limit and client/server-split classes are handled by separate Plans.

Measured at `81cf299f` with `.agents/scripts/Measure-Tokens.ps1`:

- `class ExportShader` (`DataPacker/Source/ExportJobs/ExportShader.h`): `ExportShader.cpp` 4,219 + `ExportShaderDependencies.cpp` 3,097 = ~7,316. `ExportShaderDependencies.cpp` defines `CheckDirty`, `CaptureDependencies`, and `UpdateCacheMetadata` (lines 249, 282, 326), plus its own file-local constants, `struct CachedDependencyFingerprint`, and `static` parser helpers. None of these names collide with `ExportShader.cpp`'s file-local `kbOptimizeShaders`, `GetVulkanSdkBinariesDirectory`, `BindingTable`, `WriteBinding`, or `CollectBindings`. Both files include only `ExportShader.h` and `FileManager.h`.
- `class ClientSession` (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`): `ClientSession.cpp` 2,829 + `ClientSessionReceive.cpp` 623 + `ClientSessionSubscriptions.cpp` 276 = ~3,728. `ClientSessionReceive.cpp` defines `ApplyReceivedStaticData`, `HydrateReceivedFullState`, `ResetCoordStatesForResync`, and `LogDesyncFrameDifferences`. `ClientSessionSubscriptions.cpp` defines `UpdateDesiredCoords` and `UpdateSubscriptions`. All three files wrap their bodies in `namespace game` and `#if defined(BT_CLIENT)`, and none has file-local symbols other than `ClientSession.cpp`'s `ToString`.
- `class TweaksScreen` (`Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`): `TweaksScreen.cpp` 233 plus seven section files, `TweaksScreenHexShield.cpp` 409, `TweaksScreenParticles.cpp` 2,070, `TweaksScreenSmokeDeposits.cpp` 1,348, `TweaksScreenWindDeposits.cpp` 592, `TweaksScreenLightingEffectsVisible.cpp` 1,578, `TweaksScreenLightingEffectsLighting.cpp` 1,724, `TweaksScreenSoundEffects.cpp` 957 = ~8,911. Each section file holds one anonymous-namespace `const engine::TweaksSliderMapRegistrar g<Section>Registrar` and one `TweaksScreen::Render*` member. The registrar constructor (`Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.cpp:18`) inserts keys into an `unordered_map` and asserts on a duplicate key. Registration order therefore has no effect, and putting the seven registrars in one translation unit changes nothing.

All eleven `Projects/BrokenEngineSandbox` files are members of `BrokenEngineSandbox.vcxproj` only (client). The two DataPacker files are members of `DataPacker.vcxproj`. Each is listed in the matching `.filters` file.

`/reduce-file` already agrees with the rule. Its worker rule (`.agents/skills/reduce-file/references/worker.md:88`) forbids spreading one concrete class's member definitions across sibling `.cpp` files, and step 7.3 allows split implementation files only for a static-method struct, which is a `struct` and therefore exempt. Neither site becomes stale. `/code-style-review` is different: `.agents/skills/code-style-review/references/worker.md` step 7 lists every guide rule's owner, and its wording "Every other guide rule has another owner" would be false for a new rule that no list names.

The merge leaves file names in comments and docs that no longer exist:

- `Projects/BrokenEngineSandbox/Source/Ui/ParticleWrappers.h:10` names `TweaksScreenParticles.cpp`.
- The same comment at `Engine/Source/Ui/SmokeWrappersBase.h:36`, `Engine/Source/Ui/SmokeWrappersBase.cpp:34`, and `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp:36` names game-side `TweaksScreenSmokeDeposits`.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h:16` says each translation unit declares one registrar.
- `.agents/skills/add-collection-member/references/worker.md:129` names `ClientSessionReceive.cpp`.
- `Documents/Investigations/ReplayOwnershipRecipeViewerHold.md:46` names `ClientSessionSubscriptions.cpp`.

## Design

The author recommends the following.

1. Style rule. Append rule 67 after rule 66 in `Documents/C++StyleGuide.txt`, separated by one blank line, with this text:

   `67. A type declared with the "class" keyword keeps every member definition that is not in its header in one .cpp file; never split one class's member definitions across several .cpp files. Types declared with "struct" (including frame Collections) and free functions are exempt: ClientSession.cpp defines every ClientSession member.`

2. Rule ownership. In `.agents/skills/code-style-review/references/worker.md` step 7, add `67` to the hand-read rule list ("... 62, 65 and 67"). In step 12's do-not-auto-fix list, add "which `.cpp` file holds a definition", so a rule 67 violation is reported under `Routed Findings` and never merged as a style fix. Merging moves project membership, which is not a meaning-preserving style edit.

3. `ExportShader`. Append the body of `ExportShaderDependencies.cpp` (lines 4-347: constants, `CachedDependencyFingerprint`, `static` helpers, and the three members) to the end of `ExportShader.cpp` without changing it. Delete `ExportShaderDependencies.cpp`.

4. `ClientSession`. Add `ClientSessionReceive.cpp`'s three Collection includes to `ClientSession.cpp`. Paste the four member definitions from `ClientSessionReceive.cpp`, then the two from `ClientSessionSubscriptions.cpp`, unchanged, just before `ClientSession.cpp`'s `#endif // BT_CLIENT`. Delete both files.

5. `TweaksScreen`. Merge the include lines of all seven section files into `TweaksScreen.cpp`. Inside its `namespace game` block, after `RegisterGameTweakSections()`, add each section's registrar and render member in `TweaksScreen.h` declaration order: HexShield, Particles, SmokeDeposits, WindDeposits, LightingEffectsVisible, LightingEffectsLighting, SoundEffects. Keep one registrar per section, next to its render member. Do not copy the anonymous `namespace { }` wrappers. Moved lines count as session-added under rule 66, and a namespace-scope `const` registrar already has internal linkage, so it takes no keyword. This matches the engine registrars, for example `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWind.cpp:11`. Delete the seven section files.

6. Includes. Run the rule 47 fixer exactly as `.agents/skills/code-style-review/references/worker.md` step 7 documents it over the three merged files. The corresponding header goes first in its own group.

7. Membership. Route the ten deleted `.cpp` files through `/update-vcxproj`, which removes them from `DataPacker.vcxproj` or `BrokenEngineSandbox.vcxproj` and their `.filters` files. Do not hand-edit project XML.

8. Stale references:
   - `ParticleWrappers.h:10`: point to the slider order in `TweaksScreen::RenderParticlesSection()`.
   - The three smoke comments: name the game-side `TweaksScreen::RenderSmokeDepositsTab()`.
   - `TweaksSliderMap.h:16`: change "each translation unit declares one of these" to "each tweaks section declares one of these".
   - `add-collection-member` worker line 129 and the Investigation line 46: name `ClientSession.cpp`.

Change Workflow tier: Tier 1. Trigger: documentation, project membership, and moving definitions without changing behavior, with no public signature or invariant exposure. Determinism/CRC, wire, serialization, `.pack`, `kiVersion`, replay, threading, allocation, and shaders are unaffected. `ExportShader::kiVersion` stays unchanged because the exported bytes do not change.

## Critical files

- `Documents/C++StyleGuide.txt`
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/skills/add-collection-member/references/worker.md`
- `DataPacker/Source/ExportJobs/ExportShader.cpp`, `ExportShaderDependencies.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`, `ClientSessionReceive.cpp`, `ClientSessionSubscriptions.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp` and the seven `TweaksScreen<Section>.cpp` files listed in `## Context`
- `Projects/BrokenEngineSandbox/Source/Ui/ParticleWrappers.h`
- `Engine/Source/Ui/SmokeWrappersBase.h`, `Engine/Source/Ui/SmokeWrappersBase.cpp`, `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp`, `Engine/Source/Ui/Screens/TweaksScreen/TweaksSliderMap.h` (comment lines only)
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj(.filters)`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj(.filters)`, through `/update-vcxproj`
- `Documents/Investigations/ReplayOwnershipRecipeViewerHold.md`

## In scope

- Adding rule 67 to `Documents/C++StyleGuide.txt`.
- The `/code-style-review` worker step 7 rule list and step 12 do-not-auto-fix list.
- Moving every out-of-header member definition of `ExportShader`, `ClientSession`, and game `TweaksScreen` into `ExportShader.cpp`, `ClientSession.cpp`, and `TweaksScreen.cpp`, with the file-local constants, types, `static` helpers, and slider registrars those definitions use.
- Removing the anonymous-namespace wrappers around the seven moved registrars.
- Merging include blocks and running the rule 47 fixer on the three merged files.
- Deleting the ten emptied `.cpp` files, with their project and filter membership updated through `/update-vcxproj`.
- The stale-reference edits listed in Design step 8.

## Out of scope

- The other split classes (`Client`, `Server`, engine `TweaksScreenBase`, `IslandTerrain`, `ProfileManagerBase`) and their files, apart from the one comment line in `TweaksScreenSmoke.cpp`.
- Any change to the body or signature of a moved definition. This includes `ParseWhitespaceDependencies` and the registrar entry lists.
- Changes to headers' declarations, `ExportShader::kiVersion`, and any `struct` or free-function file layout.
- `.agents/skills/reduce-file/` text.
- Other anonymous namespaces in `Projects/BrokenEngineSandbox`, other include-order fixes, and any other style cleanup.

## Acceptance criteria

- `Documents/C++StyleGuide.txt` ends with rule 67 as worded in Design step 1.
- `.agents/skills/code-style-review/references/worker.md` step 7 lists rule 67 among the hand-read rules.
- `git ls-files` lists none of the ten deleted `.cpp` files.
- `git grep -lE '(^|[^A-Za-z_:])(ExportShader|TweaksScreen)::' -- '*.cpp'` returns only `ExportShader.cpp` and the game `TweaksScreen.cpp`.
- No `.cpp` under `Projects/BrokenEngineSandbox/Source/Network/Client/` other than `ClientSession.cpp` defines a `ClientSession::` member.
- `git grep -nE '^\s*namespace\s*\{?\s*$' -- Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp` returns nothing.
- `Test-IncludeOrder.ps1 -Path` over the three merged files reports `status: pass`.
- `/update-vcxproj` validation passes.
- `/compile` builds DataPacker Release|x64, the BrokenEngineSandbox client, and the server cleanly.
- No live verification is needed: definitions only move, and registrar order does not matter.

## Coordination

No dependencies or mandatory coordination constraints. Related Plans, valid in either landing order:

- `Documents/Plans/Engine/ShaderDependencySpanStream.md` edits `ParseWhitespaceDependencies` in `ExportShaderDependencies.cpp`. If this Plan lands first, that function is in `ExportShader.cpp` with the same body, so locate it by symbol.
- `Documents/Plans/Game/SplitCppCorrespondingHeaderOrder.md` reorders includes in `ClientSessionReceive.cpp`, `ClientSessionSubscriptions.cpp`, and the seven `TweaksScreen<Section>.cpp` files. Its Design already skips removed paths, and its `-All` check holds after this merge. If it lands first, this merge takes the reordered blocks.
- `Documents/Plans/Engine/TweakSectionRenderFreeFunctions.md` brings engine `TweaksScreenBase` under rule 67 by turning its ten section renderers into free functions. It edits the renderer at `TweaksScreenSmoke.cpp:46` but not the comment at line 36 or `TweaksSliderMap.h:16`. Whichever lands second applies its edits by content, not by line number.
- `Documents/Plans/Game/RemoveGameAnonymousNamespaces.md` converts the same seven registrar blocks. Whichever lands second finds them already converted, and that Plan's grep-based acceptance holds either way.

## Notes

- The merged sizes stay under the 10,000 `.cpp` threshold, so `/reduce-file` does not apply.
- `TweaksScreen.cpp` and `ClientSession.cpp` stay client-only (`BT_CLIENT`), so membership in `BrokenEngineSandboxServer.vcxproj` is unchanged.
