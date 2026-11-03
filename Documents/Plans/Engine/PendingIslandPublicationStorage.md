<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:17:55.393Z","dependsOn":[]} -->
# Resolve pending island publication storage

## Context

The style-lint cleanup left publication storage deferred at the user's explicit
request. `Documents/C++StyleGuide.txt:206` rule 32 prefers `std::unordered_map`,
while `DataPacker/Source/ExportJobs/Island/PublishedIslandBake.cpp:16` retains
`std::map<std::filesystem::path, nlohmann::json> sPendingPublications`.
This Plan records that residual; the user deferred both the container choice
and its implementation, so neither option below is selected.

The map supplies unique route keys, replacement through `insert_or_assign`
in `UsePublishedIslandBake` (line 135), erasure after successful immediate
publication in `PublishIslandBake` (line 202), and ordered traversal in
`FinalizePublishedIslandBakes` (lines 205-213). A finalization exception leaves
the pending collection uncleared, after the same successfully written prefix.
`ResetPublishedIslandBakes` clears pending state explicitly (lines 161-165).
The ordered container therefore controls observable publication and failure
order even though the cache lifecycle contract does not mandate that order.

## Design

Obtain the user's storage choice before implementation. Preserve the current
map and behavior until that choice is approved. The two deferred options are:

- `std::unordered_map` plus a transient vector of route keys sorted using the
  same `std::filesystem::path` ordering as the current map before finalization.
  Lookups, replacement, and erasure have expected constant-time cost; finalizing
  requires sorting and temporary key copies.
- A sorted vector of route/identity entries, with `lower_bound` for lookup,
  replacement, insertion, and erasure. This keeps one collection and naturally
  ordered traversal, with linear insertion and removal costs.

Either option must preserve unique-key replacement, successful immediate
publication followed by pending erasure, and finalization's route order,
successful prefix, first failing route, and clear-only-after-complete behavior.
Keep `WritePublication`'s existing sorted leaf indices and publication bytes.
No performance improvement is claimed without measurement.

## Critical files

- `DataPacker/Source/ExportJobs/Island/PublishedIslandBake.cpp`:
  `sPendingPublications` and its consumers.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md`:
  Cache Lifecycle and Versioning contracts.
- `Documents/C++StyleGuide.txt`: rule 32.

## In scope

- Resolve the deferred choice between the two storage options with the user.
- After approval, replace `sPendingPublications` and update its uses in
  `UsePublishedIslandBake`, `PublishIslandBake`, `ResetPublishedIslandBakes`,
  and `FinalizePublishedIslandBakes` only as the chosen representation requires.

## Out of scope

Filesystem deletion/reparse-point policy, `sPublishedLeaves` redesign,
publication validation changes, failure recovery changes, concurrency changes,
Gaea execution, rebaking assets, new tests, and unrelated style cleanup.

## Acceptance criteria

- The user-approved storage choice is recorded before source implementation.
- Source review proves identical route replacement, reset, immediate-publication
  erasure timing, path ordering, and failure-prefix/clear behavior.
- `WritePublication` retains identical leaf ordering and emitted JSON bytes;
  publication schema, stage versions, `.pack` layout, and export versions stay
  unchanged because their bytes and contracts are unchanged.
- `/compile` builds DataPacker Release x64. No runtime harness criterion is
  required: ordered traversal and exception boundaries are verified by source
  inspection, and no runtime defect or reproduction run was supplied.
- Re-adjudicate the original rule-32 residual against the chosen representation
  without weakening the global style rule.

## Notes

Future implementation is Change Workflow Tier 2: scoped DataPacker publication
storage with observable filesystem write/failure order to preserve. It changes
no simulation determinism/CRC, wire, replay, serialized format, threading, or
trust boundary. Run applicable C++ propagation, correctness, style, comment,
over-engineering, documentation, and independent acceptance checks before
finalization through `/finalize-changes`.

The present change only records the user-deferred work. It deliberately leaves
the storage decision open under that explicit instruction.

## Coordination

No dependencies or mandatory coordination with existing Plans. The filesystem
policy follow-ups own separate functions and decisions; this Plan does not
extend their boundaries.
