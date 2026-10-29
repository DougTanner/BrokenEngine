## Scope and inventory

Finder-only review of the three C++ readiness-correction regions against fix-entry baseline
c1aa4847eb49a70ebb3df5c143fc7b300913c90b in
C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. The foundation
baseline cd6d410c064a5a5f905bf68efaf28493ab6fc33a is intentionally not the reviewed diff.
Get-SessionChangeInventory returned pass ok; Temp/over-engineering-review-inventory.json fixes the
reviewed regions. Concurrent prose changes and earlier foundation code were excluded.

## Removal candidates

None. All five candidate classes were checked across Lighting.h:98, LightingCapture.cpp:31-34, and LightingComparison.cpp:5,182-188,218. No new fallback, hash, option, parameter, timeout, counter, or phase exists.

## Decisive evidence

- Repository-wide git grep IsCaptureTerrainReady found one private declaration, one definition and exactly two current consumers: preparation at LightingComparison.cpp:218 and recording at LightingCapture.cpp:31. The helper shares a currently duplicated predicate and exposes no speculative API.
- Islands.cpp:210-214 writes a zero indirect indexCount for a nonresident mesh. Islands.cpp:377 nevertheless publishes the desired placement total in mLastWrittenCounts. Terrain.cpp:133 uses that total to span template runs; :151-157 skips nonresident or zero-index runs; :191 publishes only prepared placements. The mismatch is reachable during ordinary restoration, so the gate cannot be replaced with an ASSERT or deleted as impossible-state protection.
- PrepareComparisonPipelines exits before snapshot and resource preparation when pending. RecordCapture is independently called with receivers status and must exit before image transition and RecordReceivers when preparation was skipped; deleting its gate would consume unprepared or stale capture resources. Both conditions remain necessary despite using the same same-frame predicate.
- LightingComparison.cpp:207-215 retains the existing tracing publication-change failure path. The new predicate is evaluated only in receivers; empty terrain compares zero to zero and needs no special case.

## Design suggestions

None. No root-cause redesign is needed to remove useless machinery within this correction's scope.

## Verification limits

Read-only static review only; no build or runtime operation. No tracked source changes. The evidence file and inventory are review artifacts.
