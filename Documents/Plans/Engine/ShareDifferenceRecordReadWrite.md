<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:36.836Z","dependsOn":[]} -->
# Share one tick-plus-difference record reader and writer across DifferenceStream's two record sections

## Context

`Engine/Source/File/DifferenceStream.h` serializes two sections of `(int64 tick, DIFFERENCE_TYPE)` records, post-dispatch records in the header file and differences in `.frames`, and each side repeats the same per-record code twice:

- Reader: in the `DifferenceStreamReader` constructor (`DifferenceStream.h:226-484`), the post-dispatch loop (`:303-341`) and the differences loop (`:373-408`) both read the int64 tick and check `gcount`, then take an `if constexpr (std::is_trivially_copyable_v<DIFFERENCE_TYPE>)` branch: either `common::Read` with a `gcount` check, or `operator>>` with a stream-state check. Every short read logs one message and returns. That message is constant within a loop but differs between the loops: `post-dispatch section doesn't match its count` versus `Recorded frames file size doesn't match header`.
- Writer: `DifferenceStreamWriter::Save` writes the same record shape in two loops, `:94-105` and `:112-123`.

The loops differ only in their tick-range checks, which must stay separate. Post-dispatch (`:334-335`) accepts the start tick and rejects end+1; differences (`:401-402`) reject the start tick and accept end+1.

Origin: a read-only review of the top 20 functions by complexity mass at `43450112`. The user's governing rule, as relayed by the dispatching agent: a function is never split only because it is big; only a piece that is a separate job fused into its function, or a copy-paste, is moved or de-duplicated. The duplicated record read and write was judged such a piece; the rest of the constructor and of `Save` was judged earned and stays.

## Design

Author's recommendation, with rationale:

1. Add a private static `static bool ReadDifferenceRecord(std::istream& rStream, int64_t& riTick, DIFFERENCE_TYPE& rDifference)` to `DifferenceStreamReader`. Its body is the shared read: the tick read and its `gcount` check, then the `if constexpr` branch with its per-arm `gcount` or stream-state check. It returns `false` on any short read and does not log, because the two callers log different messages.
2. Each loop keeps its local declarations, its own `LOG` and `return` on `false`, its own tick-range and ordering check, and its `emplace_back`.
3. Add a private static `static void WriteDifferenceRecord(std::ostream& rStream, int64_t iTick, const DIFFERENCE_TYPE& rDifference)` to `DifferenceStreamWriter` holding the shared write. Both `Save` loops call it, so the writer and reader sides stay parallel.

## Critical files

- `Engine/Source/File/DifferenceStream.h`

## In scope

- New private static `DifferenceStreamReader::ReadDifferenceRecord` and `DifferenceStreamWriter::WriteDifferenceRecord`.
- The per-record read bodies of the post-dispatch loop (`DifferenceStream.h:305-330`) and the differences loop (`:375-400`), and the per-record write bodies of the two `Save` loops (`:96-104`, `:114-122`), at `43450112`.

## Out of scope

- Both tick-range and ordering checks and their messages, and the count validations at `:300` and `:370`.
- The initial-difference read and write (`:270-277`, `:83-90`), which carry no tick.
- Other redundant-looking checks such as the checksum count re-validation at `:438` and the span bound at `:352`: `Engine/Source/File/AGENTS.md` `## Replay Streams` lets existing replay checks stay.
- Every other member of both classes.

## Risk tier and invariants

Expected Tier 1 (trigger: a local, behavior-preserving de-duplication into private static members; no public signature, and the replay serialization surface is not altered).

- No format change: the same bytes are written and read in the same order, every rejection keeps its message, and no `kiVersion` moves.
- The two different tick-range rules stay exactly as they are.

## Acceptance criteria

- `std::is_trivially_copyable_v<DIFFERENCE_TYPE>` appears once in each record helper and no longer in the two reader loops or the two `Save` loops.
- The client and the server build through `/compile` (`Engine.h` includes the header in both).
- Through `/agent-harness`, the `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` `### Replay determinism acceptance` route passes, and a recording that captures a transfer plays back with its post-dispatch record (`### Replay transfer-capture harness rig`), so both record sections round-trip.

## Notes

- Line numbers are from `43450112`.
