<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T23:13:22.592Z","dependsOn":[]} -->
# Correct the Features/Frame citations of the deleted Targets symbols

## Context

The FrameRegistry change (`Documents/Plans/Engine/FrameRegistry.md`) deleted the
game `Targets` collection pair, the `game::target_t` handle type, and
`Frame::GetMissileTarget`, replacing them with the transient
`engine::FrameRegistry` query windows and the `puiRegistryIds` /
`puiRegistryTargets` id columns on the Spaceships and Missiles collections.

The manual `Documents/Features` tree was outside that Plan's boundary — its
`## In scope` lists only `Documents/Architecture/FrameUpdatePipeline.md`, the
subsystem `AGENTS.md` files, `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`,
and the two `add-collection*` skills — so four citations of those now-deleted
symbols were correctly left untouched and are now stale:

- `Documents/Features/Frame/FrameRelativePositions.md:33` — lists "Targets
  positions" among the world-space position arrays `OffsetPositions` must
  offset.
- `Documents/Features/Frame/FrameRelativePositions.md:85` — cites
  `GetMissileTarget` at `Frame.cpp:459-466` as the cross-collection targeting
  site that needs no change.
- `Documents/Features/Frame/FrameRelativePositions.md:90` — repeats "Targets
  positions" in the per-collection `OffsetPositions` enumeration.
- `Documents/Features/Frame/Future_CollectionVariants.txt:14` — cites
  `target_t` as a `MissilesPostRender` field in its wasted-memory estimate.

A repository-wide search for `target_t` and `GetMissileTarget` across
`Common`, `Engine`, and `Projects` returns no code matches, confirming all four
symbols are gone and these are the surviving stale references. Neither feature
document is scheduled for implementation (`Future_CollectionVariants.txt:3`
reads "Do not implement"), so the cost of the staleness is a future reader
planning against symbols that no longer exist.

## Design

A citation correction only. Each stale term is replaced with the registry
reality the FrameRegistry change landed, and the surrounding argument of each
document is preserved exactly:

- The two `OffsetPositions` enumerations drop "Targets positions". The recommended
  handling is to simply delete that list term rather than substitute anything:
  the registry copies no positions, the spaceship rows the registry binds are
  already covered by that collection's own `pVecPositions` term, and the
  `puiRegistryIds` / `puiRegistryTargets` columns are ids, not positions, so no
  offset applies to them. The author recommends against adding an explanatory
  aside here — the lists are enumerations of position arrays, and a removed entry
  needs no note.
- The `GetMissileTarget` citation becomes the registry acquisition site that
  replaced it: `engine::AcquireRegistryTargets` called from the Missile-Update
  query window. The claim the sentence makes — that cross-collection targeting
  needs no change under frame-relative positions, because it is same-frame
  relative math — still holds, since the query context binds the same frame-local
  position columns; only the named function and its `path:line` change. The
  author recommends re-deriving the new `path:line` at implementation time from
  the then-current tree rather than trusting any line number recorded here.
- The `target_t` citation becomes `engine::registry_id_t`
  (`Missiles.h:129`, `puiRegistryTargets`). The per-missile byte estimate in that
  sentence is re-derived from the current type rather than carried over.

Determinism, CRC, serialization, `.pack`, replay, wire, threading, allocation,
shader, and build exposure: none. No tracked file outside
`Documents/Features/Frame/` changes.

## Critical files

- `Documents/Features/Frame/FrameRelativePositions.md`
- `Documents/Features/Frame/Future_CollectionVariants.txt`

## In scope

- `Documents/Features/Frame/FrameRelativePositions.md:33` — remove the "Targets
  positions" term from the `OffsetPositions` position-array list, leaving the
  rest of the sentence and the other terms unchanged.
- `Documents/Features/Frame/FrameRelativePositions.md:90` — remove the "Targets
  positions" term from the per-collection `OffsetPositions` enumeration, leaving
  the rest of the line unchanged.
- `Documents/Features/Frame/FrameRelativePositions.md:85` — replace the
  `GetMissileTarget` `Frame.cpp:459-466` citation with the current registry
  acquisition site and its verified `path:line`, keeping the sentence's
  "needs no change" claim and its other listed sites intact.
- `Documents/Features/Frame/Future_CollectionVariants.txt:14` — replace
  `target_t` with `engine::registry_id_t` and re-derive the stated per-missile
  byte figure from the current type sizes.

The four cited lines are the target and the ceiling.

## Out of scope

- Redesigning, rescoping, restatusing, or re-validating either feature
  document, including any judgement about whether the frame-relative-positions
  design is still the right one.
- Auditing other citations in either document, or any other document in
  `Documents/Features`, for staleness from unrelated changes.
- Any change under `Documents/Plans`, `Documents/Investigations`,
  `Documents/Architecture`, `.agents/`, or any source tree.
- Converting either document into an executable Plan or adding plan metadata to
  it — `Documents/Features` is manual and is never a scheduler input.

## Risk tier and invariants

Expected Change Workflow Tier 1: documentation only, no public signature, no
invariant exposure, no behavior change, and no C++ or shader bytes. The highest
applicable risk trigger is "documentation" alone.

## Acceptance criteria

- `Documents/Features/Frame/` contains no occurrence of `target_t`,
  `GetMissileTarget`, or "Targets positions".
- Every replacement `path:line` cited resolves to the symbol it names in the
  tree at implementation time.
- No file outside `Documents/Features/Frame/` is changed.

## Notes

Recorded as a Change Workflow Step 7 out-of-scope residual of the FrameRegistry
session. No dependency edge: the FrameRegistry Plan's deletions are already in
the tree this Plan was written against, so this work is immediately eligible.
