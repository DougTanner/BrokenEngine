# Compact Pusher Zone CSR Grid

An open performance and memory investigation evaluating whether to replace the
dense pusher spatial-hash bucket array in
`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp` with a two-pass
flat XY Compressed Sparse Row (CSR) grid.

## Current Baseline: Dense XY Zone Grid

`PushersUpdate.cpp` stores one `thread_local` dense XY grid per simulation
thread covering the cell's bounding rectangle (`kfCellHalfWidth`,
`kfCellHalfHeight`):

- `sPusherZones`: `common::StableVector<int64_t>` sized to
  `kiPusherZones * kiPusherZones * kiMaxPushersPerZone` slots on the thread's
  first `SetupZones` call. At current constant values (`sizeof(int64_t)` per
  slot), this commits ~10.24 MB (`10,240,000` bytes) per simulation thread
  regardless of how many pushers are active.
- `sppiPushersPerZone`: `int64_t[kiPusherZones][kiPusherZones]` (`20,000` bytes,
  ~20 KB) storing per-zone counts, zeroed on every `SetupZones` call.
- Each zone caps at `kiMaxPushersPerZone` rows; overflow triggers
  `DEBUG_BREAK()` and logs one `kWarning` dropped-registration summary per
  `SetupZones` call, dropping later rows in ascending row order.

## Proposed Alternative: Two-Pass Flat XY CSR Grid

Replace the fixed-stride per-zone buckets with a compact flat entry buffer
indexed by per-zone CSR ranges, keeping all storage `thread_local`:

- `sPusherZoneOffsets`: `int64_t[kiPusherZones * kiPusherZones + 1]`
  (`20,008` bytes at `int64_t`) holding exclusive prefix sums so zone `z` spans
  `[sPusherZoneOffsets[z], sPusherZoneOffsets[z + 1])`.
- `sPusherZoneCursors`: `int64_t[kiPusherZones * kiPusherZones]`
  (`20,000` bytes at `int64_t`) holding per-zone write cursors during pass 2.
- `sPusherZoneEntries`: `common::StableVector<int64_t>` storing the packed
  pusher row indices across all occupied zones.

`SetupZones` runs two passes over `i = 0 .. rCurrent.iCount - 1`:

1. **Count and prefix-sum pass**: zero `sPusherZoneCursors` (reusing it as the
   per-zone count array), walk every pusher to compute its overlapping `(k, j)`
   XY zone range and increment `sPusherZoneCursors[z]`, then sweep zones to
   write exclusive prefix sums into `sPusherZoneOffsets` and overwrite each
   `sPusherZoneCursors[z]` with its starting offset, growing
   `sPusherZoneEntries` if the total registration count exceeds its current
   `Size()`.
2. **Scatter pass**: walk pushers in the same ascending row order
   (`i = 0 .. rCurrent.iCount - 1`) and write `i` to
   `sPusherZoneEntries[sPusherZoneCursors[z]++]` for each overlapping zone `z`.

`ApplyPush` reads the contiguous slice
`[sPusherZoneOffsets[z], sPusherZoneOffsets[z + 1])` for the query's single XY
zone. Ascending row order, single visitation per peer, and all 3D pusher math
stay unchanged, and the per-zone `kiMaxPushersPerZone` cap and overflow drop
path are eliminated.

## StableVector Committed Memory vs. Per-Tick Entry Counts

Any memory comparison must distinguish per-tick active entry counts from peak
committed bytes:

- **Per-tick active entries**: on a given tick, `SetupZones` writes
  `totalEntries = sum_i (zonesX_i * zonesY_i)` entries (typically 1-4 per
  pusher, depending on pusher radius vs. zone width).
- **Peak committed bytes**: `common::StableVector` (`Common/StableVector.h:8`)
  is grow-only — `Resize` commits pages via `MEM_COMMIT` inside a single
  `MEM_RESERVE` virtual reservation and never shrinks or decommits until the
  owning thread exits. Consequently, `sPusherZoneEntries` retains the
  high-water commit charge of the busiest tick that thread has ever simulated
  (`peakTotalEntries * sizeof(int64_t)` rounded up to OS page size), not the
  current tick's entry count.
- **Effective savings**: per simulation thread, committed memory changes from
  ~10.26 MB fixed (`sPusherZones` + `sppiPushersPerZone`) to ~40 KB fixed
  (`sPusherZoneOffsets` + `sPusherZoneCursors`) plus the thread's high-water
  `sPusherZoneEntries` commit. Even a 16,384-entry peak on a thread commits
  ~128 KiB, far below 10.24 MB, but measurements must report peak committed
  bytes per thread after stress spikes rather than average per-tick counts.

## Reservation Sizing (`iReservedCount`)

`common::StableVector(int64_t iReservedCount)` (`Common/StableVector.h:21-26`,
`:63-65`) reserves `iReservedCount * sizeof(T)` bytes rounded up to 64 KiB
allocation granularity:

- The reservation is a hard virtual-address-space ceiling that asserts
  (`ASSERT(iNeededBytes <= miReservedBytes)`) rather than relocating if
  exceeded.
- Virtual address reservation consumes no commit charge, so a generous
  `iReservedCount` is cheap in physical/commit memory.
- Sizing `iReservedCount` must account for the maximum plausible pusher count in
  a single cell multiplied by the worst-case number of XY zones a large pusher
  (such as a Battleship) can overlap, with headroom above observed stress peaks
  so normal or stress-test fleets never trip the ceiling assert.

## Open Questions

1. Under heavy ship counts (hundreds to thousands of ships in one cell,
   including dense overlapping fleets), how much total committed memory across
   all Dispatch worker and reconcile threads does the CSR grid save once peak
   high-water `StableVector` commit is reached?
2. Does the extra `SetupZones` pass (recomputing zone bounds twice, or caching
   per-pusher bounds in `gpThreadLocal->mWorkbuffer` between passes) regress
   `SetupZones` + `ApplyPush` CPU time compared to the dense grid, or does
   denser cache locality during `ApplyPush` offset or beat it?
3. Is `int32_t` worth using for offsets, cursors, and row indices (halving both
   fixed array size and peak committed entry bytes) given cell pusher counts, or
   should `int64_t` be kept to match repository loop-index conventions?

## Evaluation Method

- Run one primary non-overflowing live scripted scenario (high ship counts and
  dense fleet overlap across multiple active cells while staying within
  `kiMaxPushersPerZone` in every zone, with identical starting state and tick
  inputs) on `Profile` builds of both variants (dense XY grid vs. two-pass CSR
  grid), since `.agents/skills/agent-harness/SKILL.md` `## Inputs` requires
  `Profile` for CPU timing comparisons and reserves replay commands for `Debug`
  builds. Report any `kiMaxPushersPerZone`-overflowing stress timing separately
  so dropped dense-grid rows cannot diverge the compared workload. On each
  `Profile` run, measure:
  - Peak committed entry bytes per simulation thread (`sPusherZones` /
    `sPusherZoneEntries` `Size() * sizeof(T)` rounded up to OS page size),
    reporting fixed-array footprint (`sppiPushersPerZone` vs.
    `sPusherZoneOffsets` + `sPusherZoneCursors`) separately, and peak active
    entries per `SetupZones` call (since one thread can simulate multiple cells
    in a single tick).
  - Add temporary `SetupZones` and `ApplyPush` CPU timers in both `Profile`
    benchmark builds and measure them alongside `kCpuTimerPostRenderUpdate`
    using `query_profile` cohorts per
    `.agents/skills/agent-harness/references/command-reference.md`
    `## Durable caveats`.
- On `Debug` builds, record the primary non-overflowing scenario (identical
  starting state and tick inputs) separately on each variant, verify matching
  per-tick CRCs across both variants to confirm identical simulated work, and
  verify each build's own replay determinism (bit-identical CRCs across replay
  loops per `.agents/skills/agent-harness/references/replay.md`
  `### Replay determinism acceptance`), because changing push behavior bumps
  `PushersInterpolate::kiVersion` and `DifferenceStreamReader` rejects
  cross-version playback. If an overflowing stress scenario is also run, verify
  each variant's own replay determinism separately, since retaining previously
  dropped peers past `kiMaxPushersPerZone` on the CSR build can change
  simulation state and CRC.

This becomes a Plan in `Documents/Plans/Engine/` only if measurements show clear
committed-memory or overflow-removal benefit without regressing simulation
update performance.
