<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-27T00:17:12.494Z","dependsOn":[]} -->
# Attribute Client and Server memory before choosing reclamation changes

## Context

`Documents/Investigations/Engine/DebugClientMemoryGrowthAfterReplayCancel.md` records the completed aggregate
measurement. Its `## Current owners, low demand and attribution` identifies 3,575,037,952 Client private-commit
bytes outside the one 1 GiB eager arena at main cycle 3, without owner-level capacity/backing attribution.
This is an accounting remainder, not proven recoverable memory or a leak. Server's corresponding remainder
is 399,929,344 bytes. Lazy destinations, loader scratch, TLS storage, frame copies, graphics/driver storage and
audio pools cannot be numerically separated by the existing aggregate counters or frame queries.

The verified gap is missing owner observability, excluded from the completed measurement's runtime/harness
boundary. `## Measured phases and repetition` records a repeated-cycle plateau and a different fresh-process
settled value. The pinned origin cell reached 65 Fighters / 101 blasters / 128 missiles and then 0 / 0 / 0 for
185.675 seconds; count reduction does not establish returned commitment. The historical large replay growth
remains unproven. Preserve the completed results and their historical provenance.

## Design

The author's recommended approach is focused developer-only read-only diagnostics through existing agent
query dispatch, followed by matched workload measurements. It closes the accounting gap without choosing an
allocator or reclamation policy before the retained bytes and costs are known.

1. Read the ownership/lifetime contracts for the owners in `## Critical files`. Prepare the exact query schema,
   owner symbols and snapshot read sites under the normal Change Workflow before implementation. Reuse stored
   sizes/capacities and existing allocator/OS read APIs. Capture TLS data on its owning thread at existing
   quiescent points and publish a completed snapshot; observe loaders under existing synchronization and
   graphics/audio on their owner threads. No foreign-thread reads of mutating storage, new release paths,
   per-allocation tracing or general telemetry framework.
2. Report owner identity, process/thread/cell/frame-copy identity where applicable, sample timestamp, live
   usage, logical capacity, physical backing, reservation and commitment separately. Label non-applicable and
   unknown fields. Cover lazy pack destinations and loader scratch/read buffers; each initialized thread's
   workbuffer/collision/AreaDamage storage; simulation/interpolation/render collection copies; allocator current
   usage/commit; graphics VMA allocation statistics and audio pools. Keep counters outside persistent/CRC state.
   Distinguish device allocations from process private commit and heap payload from its arena backing.
3. Build through `/compile` and drive all launches, queries and gameplay through `/agent-harness`. Capture
   synchronized owner/process measurements at startup, load, three same-process record/play/cancel cycles and
   a fresh-process control. The observing run's ordered setup is recorded in the Investigation's
   `## Resumed measurement at 7697f59`: canonical harness claim; fresh absent AppData root; server then client
   launch; restore window; require `clientCount:1`; inspect UI and click `[+F]##Fighter`; enable audio with
   `audio_resume`; record about 600 ticks; stop; play until a new `End replay …, looping` line; cancel; require
   ordinary unpaused state; observe for more than three minutes. Repeat three cycles before releasing, then
   repeat one cycle with a fresh pair. Record current build/data identities and exact supported requests.
4. Repeat the dedicated pinned-cell decline, then another supported population increase in that same cell.
   The recorded decline used `edit_frame uiPinned:1` at origin `[0,0]`, a valid negative `fSpawnTimer`, supported
   injection, `DestroyShip` removal, normal missile expiry and a blaster-only `kDestroy` edit after missiles
   were absent. The durable evidence does not record exact injection payloads; derive valid requests from
   current harness documentation rather than treating rejected historical edits as a recipe. Record reached
   high counts, zero counts for at least 180 seconds, and regrown counts with backing/capacity snapshots.
   Disclose unrelated active-cell changes and non-atomic snapshot gaps.
5. For each owner, record initial, peak, retained low-demand and regrown bytes. Rank potential savings from
   measured retained physical backing minus needed live backing, respecting alignment and lifetime. This is
   only an upper bound until release is observed. For reached native unload/destruction/reload or existing
   decommit paths, measure actual committed-byte drop, release duration, next load/regrowth duration and
   transient peak with at least three samples per matched comparison; report ranges. For owners without an
   existing safe release path, state that fact and calculate only a source-backed recoverable upper bound.
   Distinguish measured cost, source estimate and unknown cost in every candidate row.
6. Update the Investigation with non-overlapping attribution totals and numerical remainder at each phase,
   repeatable workloads, observation overhead/consistency limits, safe owner lifetime constraints and a
   concrete recommendation. External driver/system unknowns remain an explicit remainder, never an owner
   assignment inferred by subtraction. A future implementation candidate requires material measured retained
   backing, a reachable safe release boundary and measured or explicitly bounded release/regrowth cost.
   Record supported implementation work as a separate decision-complete proposal for future approval. When
   evidence does not support a change, retain the current policy and name the exact missing evidence. This
   diagnostics Plan authorizes no reclamation implementation and promises no performance win.

## Critical files

| Paths | Bounded owner/region |
| --- | --- |
| `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`, `AgentCommandsServerGeneric.cpp` | Existing side-generic query dispatch; focused diagnostic command module only if existing module conventions require it. |
| `Common/StableVector.h`, `Common/Workbuffer.h`, `Common/Threading/ThreadLocal.h` and `.cpp` | Stored size/capacity/backing and owning-thread snapshot points for initialized TLS instances. |
| `Engine/Source/Frame/Collision.h` and `.cpp`, `AreaDamage.h` and `.cpp` | TLS collision layers, zone indices, candidate/results storage and AreaDamage capacity observation. |
| `Engine/Source/Frame/Collections/CollectionMemory.h`, `Collection.h` | Live/logical/physical collection backing observation, with distinct frame-copy identity. |
| `Engine/Source/File/PackChunks.cpp`, `PackChunkLoader.h` and `.cpp` | Lazy committed destinations, loader scratch/read buffers and existing loader synchronization. |
| `Engine/Source/Memory/GlobalAllocator.cpp` | Current allocator usage/commit statistics beside existing mimalloc statistics reads. |
| `Engine/Source/Graphics/Managers/`, `Engine/Source/Audio/` | Existing VMA and audio pool allocation/statistics read sites only; identify exact owner symbols during execution preparation before edits. |
| `Documents/Investigations/Engine/DebugClientMemoryGrowthAfterReplayCancel.md` | Add owner measurements, reconciliation, cost comparisons and recommendation; preserve prior evidence. |

## In scope

- Developer-only read-only diagnostics in the owner regions above, including the minimal completed-snapshot
  publication needed to expose those owners safely through existing query dispatch.
- Owner accounting and matched startup/load/replay/cancel/low-demand/regrowth and native-release measurements
  for Client and Server, and the Investigation evidence/recommendation described above.
- Command-reference documentation, affected `AGENTS.md` and project membership only where new diagnostic code
  requires them. Narrow query schema/interface preparation follows the applicable workflow before code edits.

## Out of scope

- Arena architecture/default sizing, eager-commit/purge settings, trimming thresholds/delays or any allocation,
  growth, shrink or reclamation policy implementation.
- StableVector/collection pointer layout, serialization/logical capacity formats, hydration or identity changes;
  asset publication/reload behavior, audio/graphics resource lifetime changes or new release paths for benchmarks.
- Claims about historical replay growth without reproduction; changing completed measurement acceptance.
- General telemetry, per-allocation tracing, unrelated harness capabilities, deployment, pagefile changes,
  signaling other sessions or deliberate memory exhaustion, and unit tests.

## Risk triggers and invariants

Future execution is Tier 3: owner observation and completed-snapshot publication span independently owned
frame/TLS, loader, allocator, graphics and audio subsystems. Preparation must confirm the tier from the actual
diagnostic surfaces before approval; the present Plan-authoring change is Tier 1 documentation only.

Preserve deterministic frame state, save/replay/CRC behavior, pointer stability, allocation ownership,
quiescence, thread safety and narrow client/server guards. Queries cannot race owner mutation or force resource
release. Read-only observation may incur bounded diagnostic overhead; measure and disclose it rather than
claiming it is free. Device memory, virtual reservation, process commit and working set remain distinct metrics.

## Acceptance criteria

| ID | Observable completion evidence |
| --- | --- |
| A1 | Every named owner family reports live usage, capacity and backing where meaningful, with identity/timestamp and source anchor. Non-applicable fields are labelled; external unknowns remain a numerical remainder. |
| A2 | Startup/load, three same-process replay/cancel cycles, fresh-process control, pinned high/zero for at least 180 seconds/regrown phases and matched reachable release/regrowth comparisons complete. Each owner row has initial/high/low/regrown observations or a specific unreached limitation; unknown/unreached values substantiate no savings or timing claim. |
| A3 | Each phase reconciles non-overlapping owner totals and remainder against process observations without counting heap payload over arena commitment or treating device allocation as process commit. Sampling cadence, non-atomic gaps, instrumentation overhead and unrelated cell changes are disclosed. |
| A4 | A ranked candidate table gives measured released bytes or bounded potential, source-backed safe boundary, release/regrowth timing and transient peak, or explicit absence of a safe measured path. At least three samples support each matched cost comparison. The result identifies evidenced future work or justified retention of current policy with exact missing evidence. |
| A5 | Required Client/Server builds through `/compile` and runtime checks through `/agent-harness` pass. Review and replay/CRC evidence establish that diagnostics remain outside deterministic state and preserve allocation behavior. Earlier Investigation evidence and historical provenance remain intact. |

## Coordination

No overlapping live Plan or mandatory nondirectional coordination constraint was found during authoring.

## Notes

This is follow-up debt for a proven owner-observability gap, not a reopened failed criterion of the completed
aggregate investigation. The completed measurement is historical evidence, not a pending scheduler prerequisite.
Keep the current allocation policy until owner and cost evidence justifies a separately approved change.
