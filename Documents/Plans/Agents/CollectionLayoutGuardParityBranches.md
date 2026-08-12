<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T02:20:27.681Z","dependsOn":[]} -->
# Fix the collection-layout auditor's guard-parity rule for two-branch accessors

## Context

`pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1` exits `1` with
`layout.violations` and ten `guard.parity-mismatch` rows. The same ten rows
reproduce from a clean extract of baseline `c539d97`, so they are pre-existing
and were not introduced by the session that recorded this Plan:

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h:77` — `BlastersInterpolate::PersistentMembers`, member `puiTypeIndices`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h:156` — `MissilesPostRender::PersistentMembers`, members `pFlags`, `pVecExplosionDirections`, `pfDeltaRotationMax`, `pfAccelerations`, `pfPitches`, `pAlignments`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:121` — `PlayersInterpolate::PersistentMembers`, member `puiPushers`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:86` — `SpaceshipsInterpolate::PersistentMembers`, members `puiPushers`, `puiTargets`

Root cause, confirmed by reading the auditor and every flagged header: all four
accessors use the established two-branch form — a whole `#if defined(BT_CLIENT)`
/ `#else` body with one `return std::tie(...)` per branch — which is also used by
`Engine/Source/Frame/Collections/Explosions/Explosions.h:197-203` and
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h:85-91`.
`Resolve-AccessorShapes` (`.agents/scripts/Test-CollectionLayout.ps1:374-375`)
keeps only entries whose guard is active for the client build, so the `#else`
branch is discarded. `Test-GuardParity` (`:572-591`) then compares the surviving
client-branch entry's guard string against the member's declaration guard string
and reports a mismatch whenever an unguarded column is listed inside the client
branch — even though the discarded `#else` branch lists that same column, so no
build actually loses it. The flagged headers compile for both builds and every
flagged column is present in both branches; the finding is an auditor-model gap,
not a header defect.

The rule still has a real defect to catch, encoded by the `guard-parity` fixture
in `.agents/scripts/Test-CollectionLayoutFixtures.ps1:206-218,260`: a column that
is declared unguarded but appears in the tuple only under `BT_CLIENT`, with no
compensating branch, so the server build silently drops it.

## Design

Make the rule reason about per-build tuple membership instead of literal guard
strings, keeping the `guard.parity-mismatch` rule name and the auditor's
read-only contract.

1. In `Resolve-AccessorShapes`, keep the client-active entry set as `Entries`
   (every other rule already depends on it) and additionally record the
   server-active entry set on the accessor, selected from the same `RawEntries`
   with the server-build guard test that `Test-ClientActiveGuard`'s negation
   already expresses (`:88-107`). Entries outside any `BT_CLIENT`/`BT_SERVER`
   guard belong to both sets.
2. Rewrite `Test-GuardParity` to flag a member only when, for one of the two
   builds, the member is declared in that build but is absent from that build's
   entry set while being present in the other build's entry set. A member absent
   from both builds' entry sets is a deliberate omission (`PersistentMembers` and
   `SharedCrcMembers` are documented subsets) and stays unflagged; a member whose
   declaration is guarded to one build and whose entry appears only in that same
   build stays unflagged.

Under this rule the ten current rows clear (every flagged column appears in both
branches) and the existing fixture still fails as designed (`pfIntensities` is
declared for the server build, absent from the server entry set, and present in
the client entry set).

The rejected alternative is rewriting the four headers into a single tie with an
inline `#if`-guarded trailing entry. That would touch four CRC-adjacent
collection headers to satisfy a tool, and would leave the auditor still unable to
read the two-branch form that Engine's own collections use.

## Critical files

- `.agents/scripts/Test-CollectionLayout.ps1` — `Resolve-AccessorShapes`
  (`:347-390`) and `Test-GuardParity` (`:572-591`)
- `.agents/scripts/Test-CollectionLayoutFixtures.ps1` — the `guard-parity`
  fixture and case list

## In scope

- The server-active entry set recorded in `Resolve-AccessorShapes`
- The `Test-GuardParity` comparison
- The auditor's header comment where it describes guard parity (`:1-14`)
- One added fixture covering a two-branch accessor whose branches both list an
  unguarded column, asserted to pass

## Out of scope

- Every collection header; this Plan changes no C++
- Every other layout rule (`members.*`, `partition.*`, `sharedcrc.*`,
  `persistent.*`, `shared.client-guarded`, `version.*`) and the parser's
  recognized declaration, guard, and accessor shapes
- The JSON envelope, exit codes, caps, and the `/add-collection` and
  `/add-collection-member` skill documents

## Risk tier and invariants

Expected Change Workflow Tier 2 — scoped tool behavior in one script, with no
runtime, determinism, CRC, serialization, wire, or build-coordination surface.
Invariants: the auditor stays read-only and never proposes tuple text; the
existing `guard-parity` fixture must still fail with the same rule name; no other
rule's output changes for the current tree.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1` exits `0`
  with `status` `pass` and `code` `ok` on the unmodified collection headers
- `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayoutFixtures.ps1`
  passes, including the existing `guard-parity` case and the added two-branch
  passing case
- No file outside `.agents/scripts/` is changed
