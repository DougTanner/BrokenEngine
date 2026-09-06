<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:20.727Z","dependsOn":[]} -->
# Saturate server agent frame-query windows

## Context

The accepted finding `CAI/shard-0044/003` identifies unbounded integer
arithmetic at the server query trust boundary.  `OptionalCount` accepts any
`int64_t` offset or limit (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:19-22`),
and `ClampWindow` adds a nonnegative limit to a positive clamped begin without
saturation (`:29-34`).  With a nonzero offset and `INT64_MAX`, the signed
addition exceeds its domain.  The resulting invalid end can make the extraction
loop emit no rows while `total` remains nonzero; the same helper feeds player,
Spaceship, missile, and Blaster extraction (`:37-129,160-202`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0044.md:92`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1065`.
Assigned source and authorities match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the unchecked arithmetic is
pre-existing, unresolved, and outside the audit work.

Impact: a supported read-only query can produce an empty or compiler-dependent
page for a nonempty frame, undermining deterministic harness inspection.

## Design

Author's recommendation: make `ClampWindow` compute a bounded half-open range
without signed overflow.  After clamping the begin to the total, compare the
nonnegative limit with `iTotal - riBegin` and saturate the end to `iTotal` when
the limit would exceed the remaining rows.  Keep the existing normalization of
negative limits and default values, and use the same helper for all four
collection extractors.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:19-129` — count parsing, window arithmetic, and extractors.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:160-202` — query command publication.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:31-35` — query-window contract.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — hostile parameter and deterministic query rules.

## In scope

- Overflow-safe `ClampWindow` arithmetic for every accepted offset and limit.
- Preserving the existing total/page schema and the shared extraction helper
  for Players, Spaceships, Missiles, and Blasters.
- Returning a bounded page or the existing validation failure without invoking
  undefined arithmetic.

## Out of scope

- A new query protocol, arbitrary hard cap unrelated to the frame total, frame
  collection ordering, or entity filtering.
- Changes to simulation state, CRC, frame serialization, or query defaults
  except where required to preserve the bounded range.
- Other agent command parameter validation.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: hostile
  agent JSON controls signed arithmetic and deterministic inspection of
  authoritative frame state.

Tier rationale: the fix is a saturating comparison inside the single
`ClampWindow` helper, fully specified in Design; the query stays read-only and
no schema, ordering, or valid-window result changes.

Preserve these invariants:

- Every accepted integer window produces defined, monotonic `[begin,end)`
  bounds within `[0,total]`.
- `total` and emitted rows describe the same collection snapshot, including
  maximal limits and offsets beyond the end.
- Querying remains read-only and no simulation CRC, save, replay, or wire data
  changes.

## Acceptance criteria

- For each of the four collection queries, `limit=INT64_MAX` at offsets zero and
  one returns all rows from the bounded begin without overflow or an empty-page
  mismatch.
- Negative limits retain their documented normalization, offsets beyond total
  return an empty page with the correct total, and valid ordinary windows remain
  unchanged.
- Server `Debug|x64` builds clean through `/compile`; the agent query scenario
  observes bounded results for every accepted integer input.

## Notes

The consolidated index records external proposition `CAI-EXT-015` for the C++
signed-overflow rule.  The Plan routes the repository arithmetic defect and
does not create a separate external-claim Plan.
