<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:18:58.092Z","dependsOn":[]} -->
# Make the texture-descriptor registry internals private

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`; searches covered `Engine`, `Projects`,
`Tools`, `DataPacker`, and `Common`.

- In `TextureDescriptors.h`, `private:` starts at line 155. Above it sit
  nine helpers (`:118-127`), four structs (`:89-114`, `:131-144`), and the
  binding maps and index state (`:146-153`). Outside
  `TextureDescriptors.cpp` and `.h`, each of those 19 names appears only in
  comments (`TextureManager.cpp:196`,
  `Engine/Source/Graphics/Objects/Pipeline.h:50,52`,
  `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:304,315`).
  Four of the helpers (`WriteBindingToSet`, `WriteImageElement`,
  `WriteFullArrayDescriptors`, `WriteArrayElementFromLive`) write descriptor
  sets with no epoch check of their own; the check lives in their callers
  (`PublishFramebuffer` at `TextureDescriptors.cpp:136`,
  `RewriteSamplerDescriptors` at `:630`). `AGENTS.md` `## Shared Contracts`
  ("The epoch records which framebuffer's sets are writable") and
  `Engine/Source/Graphics/AGENTS.md` `## Frame and Resource Lifecycle`
  ("Descriptor writes reach only framebuffers whose fence has signaled") are
  the invariants a public helper can bypass. The class already keeps its
  epoch state private.
- `TextureDescriptors.cpp:492` opens `ScopedSuppressAllocationTracking` with
  no `// Heap:` comment; it covers the five `UnregisterBindingsForKey` calls
  (`:493`, `:496`) and `mIslandSlots.erase` (`:498`).
  `UnregisterBindingsForKey` (`:620-626`), called only from those two lines,
  opens its own scope with the comment. The scope is a thread-local counter
  (`Common/AllocationTracking.h:4-20`), so the nested one does nothing while
  the outer is open: the scope that does the work has no stated reason, and
  the reason sits on a no-op. `.agents/references/cpp-conventions.md`
  (Allocation tracking) requires the `// Heap:` comment on the suppression.
- `UnregisterPipeline` holds two identical erase loops
  (`TextureDescriptors.cpp:562-576` and `:578-592`) over `mTextureBindings`
  and `mBindlessArrayConsumers`.

Originating gap: the architecture phase (dependencies lens, the public
internals) and the in-function phase (the suppression and the loops) of an
analysis of `Engine/Source/Graphics/Managers` at that commit. Unmet
criteria: no code outside `TextureDescriptors` can write a set or mutate the
binding maps without an epoch-asserting entry point; the effective
suppression carries the `// Heap:` reason, including the slot-map erase, and
no nested no-op scope remains.

## Design

Recommended:

- Move `TextureDescriptors.h:89-153` below `private:`, except `mImageInfos`,
  which stays public. No `.cpp` change follows from the move.
- Put the `// Heap:` reason on the scope at `:492`, worded to cover the
  slot-map erase too, and remove the nested scope from
  `UnregisterBindingsForKey`.
- Move the loop the two hand-written loops of `UnregisterPipeline` share
  into one local generic lambda that takes the map, and call it once per
  map; inside the lambda the loop stays as written, so its behavior is
  unchanged. This edit is cosmetic alone and is included only because this
  Plan already edits the file.

## Critical files

- `Engine/Source/Graphics/Managers/TextureDescriptors.h`
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`

## In scope

- The access level of the declarations at `TextureDescriptors.h:89-153`.
- The suppression scopes at `TextureDescriptors.cpp:492` and `:620-626`.
- The two loops in `UnregisterPipeline`.

## Out of scope

- `mImageInfos`: it has outside readers (`TextureManager.cpp:190,828`,
  `Objects/PipelineCreator.cpp:143`,
  `Objects/PipelineDescriptorWriter.cpp:105,107,505,507`).
- `TextureBindingInfo` (`TextureDescriptors.h:51`), a public parameter type.
- Holding the five island channel arrays as one indexed array.
- Any change to when or what the registry writes.

## Risk tier

Tier 2 — scoped behavior. Trigger: the public surface of a header `Engine.h`
aggregates is narrowed, which Tier 1's "no public signature" excludes. No
runtime behavior changes; the edit protects the bindless write-epoch
invariant and does not alter it. No threading, determinism, layout, or wire
exposure.

## Acceptance criteria

1. Only `mImageInfos` and the entry points remain public in
   `TextureDescriptors`.
2. The suppression in the island-slot release path has one scope, with a
   `// Heap:` comment.
3. The BrokenEngineSandbox client and ModelViewer both build through
   `/compile`. The build of both client programs is the proof that no
   outside code uses a moved name: the search above is textual.
4. Through `/agent-harness`, a client run that evicts and restores an island
   and triggers one pipeline-tier recreation shows no allocation-tracking
   break and no validation error. The supplied evidence records no harness
   setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
