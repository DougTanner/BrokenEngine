<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T18:00:15.932Z","dependsOn":[]} -->
# Inline OmnisPostRender::UpdateFacing

## Context

Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Line numbers are
from the tree this Plan was written against; locate by symbol if they moved.

`OmnisPostRender::UpdateFacing` is a private static member function declared
at `Omnis/Omnis.h:282` under the comment "Defined in OmnisCombat.cpp:"
(`Omnis/Omnis.h:281`) and defined at `Omnis/OmnisCombat.cpp:35-39`. Its body
is one rationale comment and one assignment, of the normalized difference of
`vecLookPosition` and `vecPosition` to `rVecWantedDirection`. It has one
caller, in `OmnisPostRender::Update` (`Omnis/Omnis.cpp:715`), and no
counterpart in Fighters or Battleships; a search of the `Projects`, `Engine`,
and `Common` trees finds the name only at those three sites.
`Documents/C++StyleGuide.txt` rule 49 forbids a function whose implementation
is only one assignment.

Two comments describe `Update` through this function:

- `Omnis/Omnis.h:262`, "per-omni phase work split into helpers below".
  `UpdateFacing` is the only helper below it that `Update` calls; the other
  two (`SpawnBlasters`, `SpawnDeathExplosions`, `Omnis/Omnis.h:284-286`) are
  called from `OmnisPostRender::Spawn`.
- `Omnis/Omnis.cpp:553`, "and the helpers in OmnisCombat.cpp". `UpdateFacing`
  is the only function of `Omnis/OmnisCombat.cpp` that `Update` calls.

Originating gap: the architecture phase (duplication lens) of a findings-only
analysis of the `Omnis` directory, confirmed by an independent verification
read. It is pre-existing debt. Unmet criterion: rule 49 holds for
`UpdateFacing`.

## Design

Recommended (the author's recommendation):

- Replace the call at `Omnis/Omnis.cpp:715` with the assignment, writing
  `vecWantedDirection` directly, and carry the rationale comment from
  `Omnis/OmnisCombat.cpp:37` to that site.
- Delete the definition from `Omnis/OmnisCombat.cpp` and the declaration with
  its "Defined in OmnisCombat.cpp:" line from `Omnis/Omnis.h`.
- Correct the two comments the inlining makes false: `Omnis/Omnis.h:262` and
  `Omnis/Omnis.cpp:553`, so neither says `Update` has helpers, in `Omnis.h`
  or in `OmnisCombat.cpp`.

Risk tier: Tier 1 (the author's classification): local behavior-preserving
work with no public signature or invariant exposure
(`.agents/references/risk-tiers.md`). `UpdateFacing` is declared under
`private:` (`Omnis/Omnis.h:280-282`) and has one caller, so no public
signature changes. The assignment writes `pVecWantedDirections`, which is in
`OmnisPostRender::SharedCrcMembers()`, but the expression and its operands are
unchanged and both executables compile the same source, so no computed value
changes. No layout, wire, or version change.

## Critical files

- `Omnis/Omnis.h`
- `Omnis/Omnis.cpp`
- `Omnis/OmnisCombat.cpp`

## In scope

- `OmnisPostRender::UpdateFacing`: the declaration and the comment line that
  heads it, the definition, and the call site in `OmnisPostRender::Update`.
- The two comments listed in `## Design`.

## Out of scope

- `OmnisPostRender::PreCollision`, `siCollisionLayerIndex`, and the placement
  comment above the `PreCollision` declaration in `Omnis/Omnis.h`.
- Any other statement of `OmnisPostRender::Update`, and any other move
  between the Omnis files; `/reduce-file` owns the size of `Omnis.cpp`.
- Fighters and Battleships.
- The includes of the Omnis files.

## Acceptance criteria

1. No declaration, definition, or call of `UpdateFacing` remains, the
   assignment with its rationale comment is at the former call site, and the
   two comments listed in `## Design` no longer describe `Update` as having
   helpers; the diff settles this.
2. `/compile` builds the BrokenEngineSandbox client and server.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
