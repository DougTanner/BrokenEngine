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
