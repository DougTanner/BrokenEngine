# Add a Collection Member Worker

The layout-change steps and the judgment rules the runner applies. The purpose
and the triggers live in [`../SKILL.md`](../SKILL.md).

## Steps

### Choose the variant

1. Read the applicable Frame and Collections `AGENTS.md` files, the target
   collection header and implementation, its paired collection, and all
   producers/consumers before editing. Done when each of those has been read.
2. Choose the live variant and inspect everything its `Inspect for` column
   names. Done when one row is chosen and each of those has been inspected.

   | Variant | Live exemplar | Inspect for |
   |---|---|---|
   | Shared-only game collection struct | `/Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h`: `SpaceshipsPostRender`, `SpaceshipsPostRender::Spawn`, `Spaceships.cpp` `SpaceshipsPostRender::AllocateAndCopy` | `SharedMembers()` with `Members()` forwarding to it; per-struct versions, `PersistentMembers()` carry-forward, spawn initialization |
   | Shared/client-split game pair | `/Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`, `/Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`, `MissilesInterpolate::Update` | `SharedMembers()` plus guarded `ClientMembers()`, `Members()` composition, spawn, copy, `ClientInit`, transfer send |
   | Server-visible engine pair | `/Engine/Source/Frame/Collections/Pushers/Pushers.h`, `PushersInterpolate::Sync`, `PushersPostRender::Add`, `/Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` `Frame::kiVersion` | Existing `Members()`-only shape, per-struct versions, owner sync, zero-init, difference logging, ID map |
   | Owner-synchronized client-only pair | `/Engine/Source/Frame/Collections/Sounds/Sounds.h`, `SoundsInterpolate::Sync`, `SoundsPostRender::Add` | Whole-file `BT_CLIENT`, `Members()` only, full carry-forward copy, `SyncData`/`Sync`, Add defaults, no shared version or differences |
   | Controller-driven, fire-and-forget client-only pair | `/Engine/Source/Frame/Collections/Puffs/Puffs.h`, `PuffsInterpolate::Update`, `PuffsPostRender::AddControlled` | Whole-file `BT_CLIENT`, `Members()` only, selective controller copy, unconditional animated stores, complete Add initialization |

3. Stop and report a stale exemplar if it no longer demonstrates the claimed
   variant. Done when the chosen exemplar is confirmed or the stale exemplar is
   reported.

### Place the column

4. Initialize the pointer to `nullptr` beside related columns. Done when the
   pointer is initialized.
5. Keep a client-only column and its accessor entry under the same narrow
   `BT_CLIENT` guard. Done when any client-only column and its accessor entry
   share one guard.
6. Insert the column once, into the accessor the target already uses.
   Wire/layout order is semantic, so the tuple position is yours to judge.

   - Do not introduce `SharedMembers()` merely because the collection is
     server-visible.
   - Keep a C-array of pointers as one tuple entry, since the collection helpers
     visit its elements.
   - The tuple drives allocation, growth, swap/remove, build-local read/write,
     and normally shared CRC/read; omission corrupts layout.
   - The collection-layout auditor (step 16) settles the subset and guard
     relations.

   Done when the column appears exactly once in that accessor at the judged
   position.
7. Bump the version.

   - Version bumps apply to every game collection and server-visible engine
     collection.
   - Each live version-bearing struct contributes its term, so bumping the
     struct changes the sum.
   - Pure client-only engine collections such as Sounds and Puffs have no
     version term and do not change persisted shared layout.

   Done when the applicable struct's version is bumped, or the collection is one
   that carries no version term.
8. Decide CRC membership.

   - A shared member normally reaches `SharedCollectionCrc()` through existing
     `SharedMembers()` or `Members()`.
   - A collection with `SharedCrcMembers()` needs an explicit include-or-exclude
     call that preserves the subset relation.
   - This is part of the rules that keep the simulation bit-identical across
     client and server, so if intended membership is unresolved, classify the
     decision Tier 3 (see root AGENTS.md, Risk tiers) and stop for user
     direction.

   Done when the member's CRC membership is settled or the Tier-3 decision is
   with the user.
9. Decide difference logging.

   - `LogDifferences()` coverage may intentionally differ from CRC membership,
     so follow the collection's live diagnostic intent.
   - Use `common::LogDifference<"name">` for scalar-like values and
     `common::LogDifference_Vec` for vectors.
   - Client-only collections may have no difference logger.

   Done when the column is either logged through the matching helper or
   deliberately left out.

### Carry, initialize, and propagate

10. Trace how every row receives and retains the value; allocation itself stays
    automatic through the tuple. Which mechanism carries the value forward is
    the judgment call, and it differs per collection:

    - `AllocateAndCopy()` copies only state that must carry forward there.
    - An Update-owned column is instead loaded from the previous frame and
      stored into current storage on every iteration, with the store
      unconditional and after branches and early-out decisions unless a
      documented transition-only copy path owns persistence.
    - Some transition-only, controller, and identity columns are copied instead.
    - Owner-fed state travels through `SyncData` and `Sync()`, and every
      `Sync()` aggregate initializer and caller has to agree.
    - Follow the target's live pattern: Spaceships carries its persistent
      columns through `PersistentMembers()`, Pushers copies in Update, and
      Sounds copies in `AllocateAndCopy()`.

    Done when the chosen carry-forward mechanism is wired and every affected
    initializer and caller agrees.
11. Initialize every new slot in the collection's real creation API, because
    freshly grown memory holds nothing.

    - That API is game `Spawn`, engine `Add`, or controller `AddControlled`, on
      both sides of paired storage.
    - Extend `SpawnInfo` and its callers whenever the value is supplied
      externally.

    Done when both sides initialize the column and every externally supplied
    path carries it.
12. Decide whether the value must survive cross-cell transfer; if it must, the
    whole path moves together:

    - the `TransferData` declaration and its member tuple when applicable;
    - source `TransferRequest` construction;
    - wire serialization/deserialization and payload-size accounting when the
      field crosses the network;
    - `/Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp` receive mapping
      plus destination `SpawnInfo` and spawn assignment.

    Use Blasters for the live send/receive shape. Match existing client guards;
    do not invent a second receive path. Done when transfer is either fully
    wired end to end or explicitly decided against.
13. Create a client-owned handle or resource in per-row `ClientInit`.

    - Make it survive `ClientInitAll` full-state hydration through
      `/Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp`,
      local spawn, and teardown/removal.
    - Copy it only where ownership persists.

    Done when each of those paths handles the resource, or the column owns no
    client resource.
14. Decide the column's identity semantics.

    - For `CollectionFlags::kIdToIndex`, tuple membership makes swap-and-pop
      move the column automatically.
    - If the new value changes identity/key semantics, update map construction,
      lookup, removal, and comparisons; otherwise make no ID-map edit.

    Done when identity semantics are decided and any required map edit is made.
15. Decide whether a server-visible game field belongs in the deliberately
    minimum-and-cheap agent result.

    - Current exposure lives in
      `/Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
      through `ExtractPlayers`, `ExtractSpaceships`, `ExtractMissiles`, and
      `ExtractBlasters`.
    - Record an intentional exclusion when no scenario needs it.

    Done when the field is exposed there or the exclusion is recorded.

### Verify

16. Run the collection-layout auditor from the repository root:

    ```powershell
    pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1
    ```

    Sweeps, shell-specific invocation, exit codes, truncation, JSON shape, and
    the blocking rule:
    [`../../../references/collection-layout-auditor.md`](../../../references/collection-layout-auditor.md).
    Done when the run is clean.

17. Complete the checklist. Done when every box holds:

    - [ ] Pointer declared in the intended tuple position; the split/accessor
      shape the collection already used is preserved
    - [ ] Version bumped on the changed struct when applicable
    - [ ] CRC membership decided; difference logging decided independently
    - [ ] Allocation/copy or unconditional Update persistence complete
    - [ ] Spawn/Add/controller initialization and all parameter callers complete
    - [ ] Owner `SyncData`/`Sync`/callers complete when applicable
    - [ ] Transfer send, wire, receive, and destination initialization complete
      when applicable
    - [ ] Client hydration, local creation, persistence, and teardown complete
      when applicable
    - [ ] Identity semantics and agent-query exposure explicitly decided
    - [ ] Paired element counts remain valid and tuple order still reads as the
      intended wire order
    - [ ] Collection-layout auditor run and clean (step 16)

## Rules

- Treat every new SOA pointer as a layout change.
- Do not infer behavior from the type name.
- Preserve the target's existing accessor and lifecycle shape; do not normalize
  it to another variant.
- There is no separate server-write path to update. Frame broadcast uses the
  same `CollectionWrite(..., cols.Members())` walk as the save format; on server
  builds, a split collection's `Members()` must equal `SharedMembers()`.
  `SharedCollectionRead()` allocates/zeros full client storage and reads the
  shared tuple.
- `DestroyElement`, `SwapElement`, growth, serialization, and allocation need no
  member-specific calls after correct tuple placement. `extern template`
  declarations and explicit collection instantiations also do not change for a
  member-only edit.

### Framework references

Consult as needed, each owning one topic:

- `/Engine/Source/Frame/Collections/Collection.h` — shared CRC/read,
  serialization, ID helpers
- `/Engine/Source/Frame/Collections/CollectionMemory.h` — tuple-driven
  allocation, growth, swap, destroy
- Engine Collections instructions: `/Engine/Source/Frame/Collections/AGENTS.md`
- Game Collections instructions:
  `/Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
