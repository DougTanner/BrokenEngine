# Style guide sweep: deferred fixes

Findings the `/sweep` `style-guide` type left unfixed because their fix lies
outside that type's `## Fix bound`, or could not be applied inside the sweep.
Each needs a user decision before any change. The sweep coordinator writes one
`##` section per sweep Plan under the Plan's file stem; `## Engine/` holds the
Engine batches `Engine_Source_Agent`, `Engine_Source_Network` and
`Engine_Source_Frame`.

## Engine/

### Out of the fix bound

- `Engine/Source/Frame/Collections/Puffs/Puffs.h:79` (rule 12) — the float
  time column would become a chrono duration, but it is in `Members()` and
  `PersistentMembers()` and serialized as raw float elements, so the change
  alters save bytes.

### Not applied for other reasons

- `Engine/Source/Frame/Collections/CollectionLifecycle.h:91` (rule 49,
  `AddIndexableElementWithId`) and `Engine/Source/Frame/FrameBase.h:126`
  (rule 3, `kCollectionCount`) — the rename needs matching edits in
  `.agents/skills/add-collection/references/worker.md`, which the Codex
  sandbox could not write.
- `.agents/skills/add-collection-member/references/worker.md:99` — still names
  `AllocateAndCopy()` for `Sounds`; the sweep renamed it
  `AllocateAndCopyMembers()`, and the Codex sandbox could not write `.agents/`.

### Declined as permitted forms

- `Engine/Source/Agent/Commands/ReplayFixtures.cpp:145` (rule 17) — the index
  is `vector::at`'s `size_type` parameter, which rule 17 permits.
- `Engine/Source/Network/Client/ReconcileReplay.h:150` (rule 56) — the finding
  renames a struct type, which rule 56 does not cover.
- `Engine/Source/Frame/Collections/HexShields/HexShields.h:36`, `:37`, `:38`
  (rule 36) — `XMFLOAT4` elements have constructors, which rule 36 permits.
