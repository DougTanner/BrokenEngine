# Style guide sweep: deferred fixes

Findings the `/sweep` `style-guide` type left unfixed because their fix lies
outside that type's `## Fix bound`, or could not be applied inside the sweep.
Each needs a user decision before any change. The sweep coordinator writes one
`##` section per sweep Plan under the Plan's file stem; `## Engine/` holds the
Engine batches `Engine_Source_Agent`, `Engine_Source_Network`,
`Engine_Source_Frame`, `Engine_Source_Graphics`, `Engine_Source_Ui` and
`Engine`.

## Engine/

### Out of the fix bound

- `Engine/Source/Frame/Collections/Puffs/Puffs.h:79` (rule 12) — the float
  time column would become a chrono duration, but it is in `Members()` and
  `PersistentMembers()` and serialized as raw float elements, so the change
  alters save bytes.
- `Engine/Source/CrashReport.cpp:101` and `Engine/Source/File/FileManager.cpp:322`
  (rule 35) — replacing the Win32 file call with `std::filesystem` changes
  error and attribute semantics, including the ordinary-file validation at
  `FileManager.cpp:322`.

### Not applied for other reasons

- `Engine/Source/Frame/Collections/CollectionLifecycle.h:91` (rule 49,
  `AddIndexableElementWithId`) and `Engine/Source/Frame/FrameBase.h:126`
  (rule 3, `kCollectionCount`) — the rename needs matching edits in
  `.agents/skills/add-collection/references/worker.md`, which the Codex
  sandbox could not write.
- `.agents/skills/add-collection-member/references/worker.md:99` — still names
  `AllocateAndCopy()` for `Sounds`; the sweep renamed it
  `AllocateAndCopyMembers()`, and the Codex sandbox could not write `.agents/`.
- `Engine/Data/Shaders/ShaderGlobalLayout.h:73`, `:229` (rule 56) — the rename
  needs matching edits in `.agents/skills/glsl-review/references/shader-footguns.md`,
  which the Codex sandbox could not write.
- `.agents/skills/code-style-review/references/style-rule-judgment/cases.json:150`,
  `:373` — fixture code still calls the removed accessor
  `gMissileLaunchVolume.Get()` (now `.mfCurrent`) and the old `PlayOneShot3d`
  argument list; updating it may change which rule the fixture expects, and
  the Codex sandbox could not write `.agents/`.
- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp:21` (rule 8) — the
  proposed nested `it` shadows the outer iterator, which MSVC C4456 rejects
  under `/W4 /WX`.
- `Engine/Source/Audio/AudioManager.cpp:219` (rule 36) — the fix would include
  the `StaticVoices` and `StreamingVoices` headers, which
  `Engine/Source/AGENTS.md` requires to stay forward-declared behind
  `std::unique_ptr`.
- `Engine/Source/File/FileManager.cpp:58` (rule 62) — the sweep split the
  `BCryptGetProperty` status check and the returned-length check into two `if`
  statements; whether they are one related-field check that should stay in one
  `if` is unresolved. Behavior is identical either way.

### Declined as permitted forms

- `Engine/Source/Agent/Commands/ReplayFixtures.cpp:145` (rule 17) — the index
  is `vector::at`'s `size_type` parameter, which rule 17 permits.
- `Engine/Source/Network/Client/ReconcileReplay.h:150` (rule 56) — the finding
  renames a struct type, which rule 56 does not cover.
- `Engine/Source/Frame/Collections/HexShields/HexShields.h:36`, `:37`, `:38`
  (rule 36) — `XMFLOAT4` elements have constructors, which rule 36 permits.
- `Engine/Source/Graphics/AnimationData.cpp:135` (rule 51) — the continuation
  already uses the required indentation before `||`.
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp:582` and
  `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:47` (rule 18) — local
  references are neither function parameters nor range-for references.
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp:13` (rule 36) —
  `PersistentWorker` is a class with a constructor.
- `Engine/Source/Graphics/Objects/Buffer.h:100` (rule 36) — the member is
  already initialized inline.
- `Engine/Source/Graphics/Managers/TextureCache.cpp:269` and
  `Engine/Source/File/DifferenceStream.h:85`, `:86` (rule 17) — the `int64_t`
  type is required by the serialized field it feeds.
- `Engine/Source/Engine.h:111`, `:121`, `:139`, `:149` (rule 15) — the deduced
  return type is an iterator, which rule 15(c) permits.

## StyleGuideSweepProjects

### Out of the fix bound

#### Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships (out-of-bound)
- F24 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:57` | Rule 12 | Defer migration of the float-seconds simulation/state model to chrono durations. Stored time columns occur at h lines 57, 65, 157, 158, 160; related spawn fields at h lines 183–184; time parameters at h lines 143, 144, 146, 149; constants at h lines 17–18 and cpp line 69; local time values at cpp lines 267, 271, 283, 286, 609, 629–631. A complete migration affects serialized collection element types and CRC-visible timer arithmetic. Do not change this representation in the sweep.

### Declined

#### Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender (declined)
- F3 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp:18` | Rule 64 | Existing comment states a threading constraint and consequence of breaking it, both explicitly permitted.

#### Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession (declined)
- F9 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:311` | Rule 18 | Local pointer variables are outside the named mandate covering read-only parameters and range-for references.

#### Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers (declined)
- F1 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:11` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F2 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:12` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F3 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:13` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F4 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:14` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F5 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:15` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F6 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:16` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F7 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:17` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F8 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:18` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F9 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:19` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F10 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:20` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F11 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:21` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F12 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:22` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F13 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:25` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F14 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:26` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F15 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:27` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F16 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:28` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F17 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:29` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F18 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:30` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F19 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:31` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F20 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:32` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F21 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:33` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F22 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:34` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F23 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:35` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F24 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:36` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F25 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:39` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F26 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:40` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F27 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:41` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F28 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:42` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F29 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:43` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F30 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:44` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F31 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:45` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F32 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:46` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F33 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:47` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F34 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:48` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F35 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:49` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F36 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:50` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F37 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:51` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F38 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:52` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F39 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:53` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F40 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:54` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F41 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:57` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F42 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:58` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F43 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:59` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F44 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:62` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F45 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:63` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F46 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:64` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F47 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:65` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F48 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:66` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F49 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:67` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F50 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:68` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F51 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:69` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F52 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:72` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F53 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:73` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F54 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:76` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F55 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:77` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F56 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:78` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F57 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:81` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F58 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:82` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F59 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:83` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F60 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:86` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F61 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:87` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F62 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:88` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F63 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:89` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F64 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:90` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F65 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:91` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F66 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:92` | Rule 36 | Constructed class global is permitted by worker step 10; step 7 covers class members.
- F67 | `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h:93` | Rule 36 | Constructed class global is permitted; dependent namespace/comment relocation is unnecessary.

#### Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers (declined)
- F1 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:11` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F2 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:12` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F3 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:13` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F4 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:14` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F5 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:17` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F6 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:18` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F7 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:19` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F8 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:20` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F9 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:23` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F10 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:24` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F11 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:25` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F12 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:28` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F13 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:29` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F14 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:30` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F15 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:33` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F16 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:34` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F17 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:35` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F18 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:36` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F19 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:39` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F20 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:40` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F21 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:41` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F22 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:42` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.
- F23 | `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h:45` | Rule 36 | Constructor-bearing class type is explicitly permitted by worker step 10.

#### Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers (declined)
- F3 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:9` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F4 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:10` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F5 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:11` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F6 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:12` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F7 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:13` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F8 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:14` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F9 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:15` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F10 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:16` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F11 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:17` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F12 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:18` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F13 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:19` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F14 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:20` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F15 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:21` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
- F16 | `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h:22` | Rule 36 | `engine::Wrapper` has a constructor, an explicitly permitted form.
