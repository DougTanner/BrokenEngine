# Add a Collection Worker

The variant, wiring, version, and verification steps, and the judgment rules the
runner applies. The purpose and the triggers live in
[`../SKILL.md`](../SKILL.md).

## Steps

### Choose the variant

1. Read the applicable root, Frame, and Collections `AGENTS.md` files. Done when
   each of those has been read.
2. Decide ownership and reachability before choosing files:
   - game-owned and server-visible;
   - engine-owned and server-visible;
   - engine-owned, whole-file client-only;
   - shared layout with additional client-owned objects.

   Done when one of those four is chosen.
3. Read every file listed for the selected exemplar:

   | Requested variant | Live exemplar files and symbols | Pattern to preserve |
   |---|---|---|
   | Shared-only game collection struct | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h`: `SpaceshipsPostRender`; sibling `.cpp` files | `SharedMembers()` plus `Members()` returning it, so a later client-only field cannot silently join the CRC; no special frame dispatch |
   | Shared game state plus client-owned state | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`: both structs; `Missiles.cpp`: `ClientInit`, `ClientInitAll`, `Transfer`; `MissilesUpdate.cpp` | guarded `ClientMembers()`, client `tuple_cat`, hydration, owned-object teardown, transfer payload |
   | Server-visible engine collection | `Engine/Source/Frame/Collections/Pushers/Pushers.h`: both structs; sibling `.cpp` files; `Engine/Source/Frame/FrameBase.h`; `game::Frame::kiVersion` in `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` | direct FrameBase storage, server tuple registration, index helpers, per-struct version terms |
   | Owner-synchronized whole-file client-only collection | `Engine/Source/Frame/Collections/Sounds/Sounds.h`: both structs; sibling `.cpp` files; `FrameBase.h` | outer `BT_CLIENT` guard, `Members()` only, owner `Sync`/Add/Remove, custom client-only identity only when required |
   | Controller-driven fire-and-forget client-only collection | `Engine/Source/Frame/Collections/Puffs/Puffs.h`: both structs; `Puffs.cpp`, `PuffsUpdate.cpp`, `PuffsRender.cpp`; `FrameBase.h` | `Members()` only, controller metadata copy, paired Add/Destroy, no owner handle or persisted version |

   Done when every listed file has been read.
4. Inspect that exemplar's frame registration, project membership, and harness
   query exposure. Done when each of those three surfaces has been inspected.
5. Stop and report the stale guidance instead of substituting another pattern
   silently if the named symbols no longer demonstrate the stated variant. Done
   when the named symbols are confirmed to demonstrate the variant or the stale
   guidance is reported.

### Write the pair

6. Explicitly invoke `add-collection-member`
   (`../../add-collection-member/SKILL.md`) for each new SOA `* __restrict`
   column and complete its layout checklist.

   - This skill owns collection-level wiring; that skill owns every column.

   Done when every new column has completed that checklist.
7. Use the tuple shape that matches reachability:
   - Shared-only game: define `SharedMembers()` and return it from `Members()`.
   - Shared/client split: define guarded `ClientMembers()`; client `Members()`
     returns `std::tuple_cat(SharedMembers(), ClientMembers())`, while server
     `Members()` returns `SharedMembers()`.
   - Server-visible engine collections whose whole layout is shared may expose
     `Members()` only; shared CRC/read helpers fall back to it.
   - Pure client-only engine collections expose `Members()` only and guard the
     entire header and implementation.

   Done when the pair's accessors match the reachability case chosen in step 2.
8. Create Interpolate/PostRender structs and their implementation files beside
   the chosen exemplar. Done when both structs and their implementation files
   are in place.
9. Give the mandatory `Update` its generic dispatch signature. Done when
   `Update` uses that signature and no no-op hook boilerplate was added.
10. Declare `extern template struct Collection<...>` for both structs and
    explicitly instantiate both in one implementation file. Done when both
    declarations and both explicit instantiations are in place.

### Register and version

11. For a game-owned pair:
    - Add forward declarations and paired `std::unique_ptr` members to game
      `Frame.h`.
    - Construct both pointers in the matching `Frame.cpp` constructors.
    - Include the header and add both objects to `GameInterpolateCollections()`
      and `GamePostRenderCollections()` in `FrameCollections.h`.
    - Preserve producer-before-consumer tuple order. A collection that produces
      state consumed later in the same phase must precede that consumer.

    Done when each of those four is in place.
12. For an engine-owned pair:
    - Include the header in `Engine/Source/Frame/FrameBase.h`.
    - Add direct members to both FrameBase structs, entries at matching
      positions in both `Collections()` tuples, and update both
      `kCollectionCount` values.
    - For server-visible state, also add both structs to the corresponding
      `ServerCollections()` tuples. For pure client-only state, guard include,
      members, tuple entries, and client counts with `BT_CLIENT` and omit it
      from `ServerCollections()`.
    - Preserve producer-before-consumer order in both tuples.

    Done when each of those four is in place.
13. Define `static constexpr int64_t kiVersion` on both structs for every game
    collection pair and every server-visible engine pair.

    - Add both terms to `game::Frame::kiVersion` in
      `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`.

    Done when both declarations and both sum terms exist.

### Settle and verify

14. Settle transfer and hydration, the first of the two decisions the
    collection-layout auditor cannot make. It checks tuple membership, subset
    and guard relations, and the version sum (step 17); it cannot reach these
    two decisions, so settle them before finishing:

    - [ ] Transferable state has matching send and receive wiring; source-owned
      client objects are removed and destination objects recreated. Shared
      collections with client-owned objects initialize them at local spawn and
      after server state arrives (`ClientInit`/`ClientInitAll` pattern).

    Done when that box holds.
15. Settle the harness query decision, the second decision that auditor cannot
    reach:

    - [ ] Inspect
      `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
      for server-visible state.
      - If scenarios need it, add the include, `Extract*`, `query_frame` count,
        `query_collection` arm, and allowed-name error text.
      - If not, record the deliberate exclusion. Pure client-only collections
        have no server query.

    Done when that box holds.
16. Resolve every cited path and symbol against the final tree. Done when each
    resolves.
17. Run the collection-layout auditor from the repository root and clear every
    violation:

    ```powershell
    pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1
    ```

    Sweeps, shell-specific invocation, exit codes, truncation, JSON shape, and
    the blocking rule:
    [`../../../references/collection-layout-auditor.md`](../../../references/collection-layout-auditor.md).
    Done when the run reports no violation.
18. Compile every affected client/server target through the repository `compile`
    workflow after the C++ and project-membership stages are complete. Done when
    every affected target compiles.
19. Run the acceptance scenario required by the task; use the agent harness only
    for runtime-observable criteria. Done when that scenario has run.

## Rules

- Follow the live exemplar closest to the requested ownership model; do not
  synthesize a collection from a generic full-header template.
- Keep client-only pointers out of `SharedMembers()`. Keep `SharedMembers()` a
  subset of `Members()` and any `SharedCrcMembers()` a subset of
  `SharedMembers()`.
- Tuple registration supplies normal phase dispatch, allocation/copy walks,
  build-local Write/Read, shared ServerRead/CRC, and output merging in
  `LogDifferences`.
- Reserve explicit per-collection Frame.cpp dispatch for a concrete special
  contract such as Players; `CollectionFlags::kIdToIndex` alone is not one.
  `engine::PushersInterpolate` proves that an indexable collection can use the
  normal tuples.
- Pure client-only engine collections do not alter persisted shared layout and
  do not contribute version terms. `PushersInterpolate` and `PushersPostRender`
  are the live engine example. The collection-layout auditor (step 17) checks
  declarations and sum terms against each other in both directions.
- Use `CollectionFlags::kIdToIndex` only when external owners need a stable
  handle. Prefer `AddIndexableElement`, `AddIndexableElementWithId`, and
  `RemoveIndexableElement`; they maintain the map during add and swap-and-pop.
  Manual identity wiring is conditional on a real alternate ID source or
  lifecycle, such as Sounds' client-only UUID stream, not on the flag itself.
- Optional hooks merged into the surrounding phase need no no-op boilerplate:
  absent hooks or hooks that cannot be called skip silently, while `Update` and
  direct/manual calls remain compile-checked.

### Framework references

Consult as needed, each owning one topic:

- `Engine/Source/Frame/Collections/Collection.h` — shared-member fallbacks,
  serialization/CRC, and indexable helpers
- `Engine/Source/Frame/Collections/AGENTS.md` — generic SOA invariants
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` — game
  lifecycle invariants
- `Engine/Source/Frame/AGENTS.md` — tuple ordering and frame serialization
