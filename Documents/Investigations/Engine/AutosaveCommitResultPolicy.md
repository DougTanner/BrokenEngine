# Autosave commit-result handling and caller policy

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CSB/shard-0057/001` in the frozen C++ Scope-Boundary Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Context

The game Save subsystem owns when server saves and autosaves run, while Engine
File owns grid-save framing and the atomic file transaction. `WriteGridSave`
returns whether the temporary file was committed to the destination. A normal
named save already returns that result from `GameSaveLoad::ServerSave` to its
caller.

## Finding under investigation

`GameSaveLoad::Autosave` is declared and defined as `void`
(`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:34-55`,
`GameSaveLoad.cpp:123-127`). It calls `engine::WriteGridSave` for
`ServerAutosave.save` but discards the returned commit bool. The same writer
computes that bool from `FileManager::WriteFileAtomically`, logs the commit
state, and returns `false` when the stream or the final rename fails
(`Engine/Source/File/GridSave.h:22`, `GridSave.cpp:12-47`,
`FileManager.cpp:333-359`). The failure is therefore logged, but it is not
returned to either reachable autosave caller.

The periodic path reaches `Autosave` from `GameSaveLoad::TickAutosave`, then
resets its timer and logs that the autosave fired regardless of the commit
result (`GameSaveLoad.cpp:129-142`; `Engine/Source/GameBase.cpp:451`). The
server shutdown path also calls the void entry point and continues its shutdown
sequence without a status (`Engine/Source/Main.cpp:461-480`). This is a Save
orchestration contract mismatch, not a claim that the atomic writer is silent
or unsafe.

## Controlling contract and boundary

`Projects/BrokenEngineSandbox/Source/Save/AGENTS.md:9` requires the Save
subsystem to return the actual commit result to callers, while
`Engine/Source/File/AGENTS.md` assigns FileManager the versioned and atomic
write mechanics. `FileManager::WriteFileAtomically` writes a sibling
`.tmp`, closes and checks the stream, then renames it; stream or rename failure
removes the temporary file and leaves the previous destination intact
(`Engine/Source/File/FileManager.h:179-182,268-297`,
`FileManager.cpp:333-359`). The investigation boundary is the status and
policy handoff from that existing writer through Save's periodic and shutdown
callers. It does not transfer atomic publication ownership to Save.

## Impact

When an autosave commit fails, the periodic caller cannot distinguish a
successful save from a failed attempt through the Save API. It resets the
one-hour timer and reports the firing event even though the destination was not
replaced. Shutdown likewise continues with no caller-level result. Existing
FileManager and GridSave diagnostics identify stream/open/rename failures, so
the open question is what callers should do with a known failure, not how to
invent a second atomic-write path.

## Open policy choices

No caller-response policy is selected. These choices can be combined, but the
owning authority must decide the result type or status contract and the
combination of periodic and shutdown responses. The actual commit result must
be propagated from `Autosave` to its periodic and shutdown callers; that
propagation is required, not an open policy choice. All choices below assume
that the actual result reaches both callers.

1. **Best-effort continuation after status propagation.** Return the actual
   `WriteGridSave` result from `Autosave`, let periodic and shutdown callers
   observe it, and preserve the current best-effort continuation. This is the
   smallest API correction and keeps timer and shutdown timing stable, but a
   failed periodic save still waits for the next interval and shutdown still
   exits unless callers add a response.
2. **Retry with a bounded backoff.** Return the result while retrying a failed
   periodic or shutdown commit according to a defined attempt limit and delay.
   This may recover from transient storage errors, but requires ownership of
   retry state, timer/reset semantics, shutdown wait limits, and duplicate
   failure logging without weakening each attempt's atomic transaction.
3. **Abort or alter shutdown after failure.** Treat a failed final autosave as
   a shutdown failure, or keep the process alive until a defined retry or user
   decision succeeds. This makes persistence failure harder to overlook, but
   needs an explicit exit/lifecycle contract for unavailable or unwritable
   storage and must not leave the server half-shut down.
4. **User-visible notification.** Surface the failed commit through the
   server display, console/agent result, or another existing operator channel.
   This improves recoverability without changing file semantics, but requires
   choosing the owner, lifetime, wording, and how notification supplements the
   caller's propagated status response.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md:3-24` — Save owner and
  return-result contract.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:34-55` — public
  save/autosave declarations.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:68-76,123-142` —
  result-bearing named save, result-discarding autosave, and timer behavior.
- `Engine/Source/File/GridSave.h:22-24` and `GridSave.cpp:12-47` — atomic
  grid-save result and existing commit log.
- `Engine/Source/File/FileManager.h:179-182,268-297` — one-shot atomic write
  contract.
- `Engine/Source/File/FileManager.cpp:218-253,333-359` — backup, stream,
  rename, cleanup, and failure logging.
- `Engine/Source/GameBase.cpp:433-452` — periodic caller.
- `Engine/Source/Main.cpp:461-480` — server shutdown caller.

## In scope

- Decide only the result type or status contract for the mandatory propagation
  of the actual atomic commit result from `Autosave` to its periodic and
  shutdown callers.
- Decide periodic failure behavior: timer reset, retry/backoff, next-attempt
  timing, and operator visibility.
- Decide shutdown failure behavior: continuation, bounded retry, exit result,
  or an explicit operator decision.
- Trace existing Save, Main, and File ownership so the selected policy has one
  caller owner and preserves current failure diagnostics.
- Define an observable failure scenario for stream/open and rename failure,
  including the periodic and shutdown paths.

## Out of scope

- Changing grid-save bytes, versioning, serialization order, staged load,
  adoption, or replay format.
- Replacing, weakening, or duplicating FileManager's `.tmp`/rename atomic
  transaction, backup behavior, cleanup, or existing error logs.
- Changing the save filename, autosave interval, manual quicksave/load policy,
  or unrelated FileManager write callers except where the chosen status API
  proves a required caller update.
- Adding a new notification system, retry framework, or shutdown protocol
  before its ownership and behavior are selected.
- Source, asset, build, project-membership, or scheduler changes as part of
  this record.

## Invariants

- A successful grid save continues to publish through the existing atomic
  temporary-file and rename transaction.
- A stream or rename failure leaves the prior destination intact when the
  existing FileManager contract says it does, removes the temporary file, and
  retains the current FileManager/GridSave diagnostics.
- The actual commit result cannot be discarded at the Save-to-caller boundary;
  any selected continuation, retry, abort, or notification policy must be
  based on that result.
- Autosave failure does not mutate live simulation state or the save format.
- Periodic and shutdown callers have explicitly defined behavior for both
  success and failure; neither path reports an unqualified successful commit.

## Decisive questions and acceptance evidence

- Given the mandatory propagation of the actual commit result, should
  `Autosave` return a `bool` or a richer status, and which caller owns the
  response policy after that value is returned?
- On a periodic failure, should the timer reset after the failed attempt, stay
  eligible for a retry, or enter a bounded backoff? What prevents a persistent
  disk failure from causing a hot retry loop?
- On shutdown failure, should the server continue exiting, retry for a bounded
  time, expose a non-success exit result, or wait for an operator decision?
- Which existing operator channel is authoritative for a user-visible failure,
  and how does it coexist with the current FileManager/GridSave logs?
- Can a focused scenario force a stream/open failure and a final rename
  failure with an existing valid autosave, prove the previous file remains
  intact, and observe the selected result at both periodic and shutdown
  callers?
- Does the same scenario prove that successful autosaves remain byte-valid and
  that each retry (if selected) is a separate atomic attempt with temporary
  cleanup and no changed save/replay compatibility?

The eventual executable Plan must select the caller status and failure policy,
name the exact Save/Main regions, and bind these acceptance signals. The future
tier must be re-evaluated after the choice: status propagation with local
caller handling may remain a scoped Save/Main change, while a new shutdown or
cross-subsystem persistence contract may be Tier 3.

## Provenance

- Frozen candidate: `CSB/shard-0057/001`.
- Frozen triage: `Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/triage-0001.md`
  (`Sol Triage 0001`, COMPLETE).
- Frozen Sol final disposition: `ADVISORY_DROP`, with no tracked record
  recommendation.
- A later manager judgment accepted this finding as worth tracking, and the
  user authorized this Investigation before landing. This record is therefore
  manager/user-authorized follow-up, not a triage-routed record.
- Frozen source report:
  `Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/shard-0057.md`.
- Frozen source tree: `98a34f7ffae57858863b90f7d1f9c32be268ac5a` from the audit
  commit; current checkout was verified at `c54ed87208df37d0607acbd1e98bd1d1aca7f2d6`.
- No exact duplicate was found in the live `Documents/Plans/` or
  `Documents/Investigations/` trees. The duplicate search covered
  `Autosave`, `GameSaveLoad::Autosave`, `WriteGridSave`,
  `ServerAutosave.save`, atomic-commit result propagation, and the periodic or
  shutdown failure outcome.
- No source, asset, build, or scheduler change is part of this investigation.
