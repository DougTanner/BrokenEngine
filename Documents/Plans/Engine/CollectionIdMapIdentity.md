<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:33:20.229Z","dependsOn":[]} -->
# Validate indexable collection IDs against paired rows on read

## Context

The accepted finding `CAI/shard-0017/001` identifies a collection identity gap
at the save/replay/full-state boundary. `OptionalIdToIndex::Read` verifies map
cardinality, unique keys, and a permutation of row indices, but never checks
that each key is a valid ID or equals the ID stored in the paired PostRender
row (`Engine/Source/Frame/Collections/Collection.h:206-253`).
`Frame::operator>>` validates only count/capacity parity before moving the
loaded frame (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:829-840`).
A bijective swapped map can therefore retarget normal updates or swap-and-pop
destruction to another entity without a read failure or CRC mismatch.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The cross-column
identity check is unresolved, pre-existing, and outside the approved audit
work.

## Design

The author's recommendation is to validate each indexable collection after
both its Interpolate map and paired PostRender ID column have been deserialized,
at the existing frame adoption gate. For every map entry, reject an invalid
sentinel and require the key to equal the paired row ID at the mapped index;
retain the existing cardinality/permutation checks. Throw the existing
corrupt-stream exception before `loadedFrame` is moved into live state. Keep
stable-ID lookup and the normal swap-and-pop lifecycle unchanged for valid
frames.

## Critical files

- `Engine/Source/Frame/Collections/Collection.h:206-253,338-353` — map read and collection metadata validation.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:829-840` — paired frame read/adoption gate.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:242-303` — paired PostRender IDs (read-only shape reference).
- `Engine/Source/Frame/Collections/CollectionId.h:75-85` — ID sentinel/read semantics.
- `Engine/Source/Frame/AGENTS.md` and `Engine/Source/Frame/Collections/AGENTS.md` — identity and serialization contracts.

## In scope

- Cross-checking every indexable Interpolate `idToIndexMap` key against the
  exact ID in its paired PostRender row before frame adoption.
- Rejecting invalid/sentinel IDs and mismatched row identities through the
  existing save/replay/network corrupt-input path.
- The generic map-read and game frame adoption regions named above.

## Out of scope

- Changing ID generation, map serialization order, collection layout, CRC
  formulas, swap-and-pop removal, or valid producer data.
- Registry/type-index validation (owned by `CollectionTypeIndexValidation.md`),
  count/capacity bounds, gameplay identity policy, or protocol/version changes.
- Repairing a corrupt map by rewriting keys or rows.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). The change validates
serialized save/replay/network state and protects deterministic collection
identity/lifecycle.

Preserve these invariants:

- Every accepted indexable map is a bijection between valid paired-row IDs and
  their current row indices.
- A malformed map is rejected before any frame replaces live state; valid maps
  retain their current lookup and CRC behavior.
- Client/server collection tuple order, wire layout, and swap-and-pop semantics
  remain unchanged.

Tier rationale: the change adds a pre-specified read-only cross-check of each
map key against its paired row ID at the existing frame adoption gate, throwing
the existing corrupt-stream exception. It reads no new bytes and writes none,
so serialization layout, CRC, and every valid frame's behavior are unchanged.

## Acceptance criteria

- A structurally valid frame with two paired IDs and a swapped map is rejected
  before adoption; a zero/sentinel key is rejected as well.
- A valid frame with aligned IDs still loads, updates, destroys, and transfers
  rows through the existing map path.
- Client and server `Debug|x64` builds clean through `/compile`; save/full-state
  read exercises cover the paired identity gate.

## Coordination

`Documents/Plans/Engine/CollectionTypeIndexValidation.md` also touches the
collection read/post-read boundary. Keep ID/map cross-checks separate from
registry-index validation, preserve the existing corrupt-stream gate, and
re-derive line ranges before implementation. No dependency is required.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
