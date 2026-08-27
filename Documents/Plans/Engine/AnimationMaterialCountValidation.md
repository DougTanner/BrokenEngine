<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:56.419Z","dependsOn":[]} -->
# Fix: Bind animation material counts to the enclosing scene

## Context

The accepted survivor `CAI/shard-0027/001` shows that
`AnimationData::Load` validates its own material count only against the global
maximum (`Engine/Source/Graphics/AnimationData.cpp:65-72`).  The eager loader
derives the animation section from the scene header and passes the remaining
chunk bytes to `Load` (`Engine/Source/Graphics/AnimationData.cpp:478-491`),
but never supplies the scene material count for an equality check.  Player and
Spaceship renderers pass the scene header count to
`SkinnedMaterialCount`/`EvaluateAnimation`, which index the animation-owned
material arrays (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:261-281`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:101-196`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0027.md:59`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:775`.
Its 14 target rows all matched the frozen baseline and all non-material-count
paths were refuted.  The issue is pre-existing at frozen commit
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this routing session has not
changed source.

Impact: a readable scene whose scene count exceeds the animation count can
reach out-of-bounds material and relative-transform reads during normal client
rendering instead of failing at the pack trust boundary.

## Design

Author's recommendation: pass the enclosing scene's material count into the
animation load boundary, reject the chunk unless it equals
`AnimationHeader::uiMaterialCount`, and require the existing bounded cursor to
finish exactly at `iAnimationBytes`.  Keep aliases and precomputed arrays
private to a successful load.  The renderer call sites continue using the
scene count because equality has then been established once at load time; do
not add per-frame smaller-count clamps that would hide a corrupt pack.

## Critical files

- `Engine/Source/Graphics/AnimationData.h:8-36` — load declaration and
  animation-owned material arrays.
- `Engine/Source/Graphics/AnimationData.cpp:39-206` — header/count checks,
  bounded section cursor, aliases, and precomputed transforms.
- `Engine/Source/Graphics/AnimationData.cpp:478-491` — scene-to-animation
  section handoff.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:261-281`
  and `.../Spaceships/SpaceshipsRender.cpp:101-196` — scene-count consumers.

## In scope

- Adding the expected scene material count to the animation loader contract and
  rejecting mismatches before `mpMaterialInfos` or
  `mpAlignedRelativeTransforms` are published.
- Enforcing exact consumption of the animation section's already-authoritative
  byte extent, including the existing alignment rule.
- Keeping the current renderer loops, eager chunk lifetime, and structural
  count/index validation intact.

## Out of scope

- DataPacker's producer, which already writes both counts from the same
  material-info vector.
- Animation interpolation semantics, skeleton topology, keyframe policy,
  model texture CRC resolution, or unrelated pack-format changes.
- Backward compatibility for a deliberately mismatched current-format asset;
  it must be rejected as corrupt.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: the change
hardens a pack-backed serialized section and its cross-subsystem renderer
consumers; serialization, opaque asset trust, and client GPU publication are
Tier-3 surfaces.

Tier rationale: the fix passes an already-available scene count into one
loader and rejects a mismatch at that single boundary, with no `.pack` layout,
producer, or renderer change; valid assets load byte-identically, so only
corrupt input takes a new — and already existing — failure path.

Preserve these invariants:

- Scene and animation material counts describe one identical material set before
  any render alias or evaluator call is reachable.
- Every alias advance remains within `iAnimationBytes`, and the final cursor
  consumes the complete section without accepting hidden trailing bytes.
- Valid DataPacker output still loads with the same material order and
  animation results; no server/PostRender CRC or wire data changes.

## Acceptance criteria

- A current-format scene with unequal scene/animation material counts is
  rejected as a corrupt eager chunk before client rendering begins.
- A valid animated scene loads and both Player and Spaceship render paths use
  all scene materials without an out-of-bounds read.
- A chunk with a bounded header but trailing or truncated animation-section
  bytes is rejected by the same loader boundary.
- Client Debug and Release builds pass `/compile`; a harness boot with the
  shipped animated assets has no new asset-load or render error.
- No per-frame renderer clamp or alternate material count is added, and no
  `.pack`, replay, save, or wire revision is introduced.

## Notes

The audit separately mapped exporter filtering and skeleton-order concerns to
other candidates; this Plan owns only the scene/animation material-count
identity boundary.
