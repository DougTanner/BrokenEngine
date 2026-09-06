<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T21:05:18.454Z","dependsOn":[]} -->
# Reduce PackChunks.cpp below the implementation-file size threshold

## Context

`Engine/Source/File/PackChunks.cpp` is over the repository's implementation-file
reduction threshold. Measured with the repository-owned tool at this Plan's
authoring baseline `9c91258e550f958fe529dd9b48db8afd56865755`:

```
pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'Engine/Source/File/PackChunks.cpp'"
-> Lines 1181, Bytes 48402, Tokens 12101 (bt-token-v1)
```

`/reduce-file` sets the `.cpp` reduction threshold at over 10,000 bt-token-v1
(`.agents/skills/reduce-file/references/worker.md`, step 2), so the file is 2,101
tokens over.

The overage is pre-existing. The same measurement on the file's content at that
baseline commit reports 11,514 bt-token-v1 — already 1,514 over — before the
authoring session's own edits, which added roughly 587 tokens. Reducing the file
was outside that session's approved implementation boundary, so its
`/repo-code-review` reviewer reported the size observation and deferred planning,
as `/reduce-file` directs for a review-time size finding.

That reviewer found no cohesive split exposed by the changed regions, so this
Plan deliberately does not pick a boundary; choosing one is the reduction
analysis this Plan schedules. Two measurements show why the choice needs that
analysis rather than a guess: the chunk-flag decoding and header/location
validation block (`PackChunks.cpp:96-243`) is 1,395 tokens and the three memory
statistics accessors (`PackChunks.cpp:1099-1172`) are 607, so even moving both
together leaves the file at roughly 10,099 — still over. A reduction that
actually clears the threshold has to take a larger cohesive responsibility, and
which one that is depends on the shared state and include consequences
`/reduce-file`'s mapping step establishes.

The file is `FileManager`'s private packed-asset implementation
(`Engine/Source/File/AGENTS.md`, `## Packed Assets`). It carries several
candidate responsibility groups — pack/manifest open and validation, the
background loading threads and per-chunk decode, the lazy range
reload/decommit/recommit surface, texture chunk state reset, and memory
statistics — which is why a cohesive boundary is plausible but must be chosen
against the then-current file.

No live Plan owns this file's size. Several live Plans edit regions of `PackChunks.cpp` for
behavior reasons; none of them changes its length. See `## Coordination`.

## Design

The author's recommendation is to run `/reduce-file` on
`Engine/Source/File/PackChunks.cpp` in its standalone analysis mode, take the
boundary that skill's mapping and reduction-preference steps select, and then
execute exactly that boundary as a behavior-preserving code move.

Recommended route rather than prescribing a split here: `/reduce-file` owns the
qualification, mapping, and least-disruptive-reduction preference order — move
independent free functions, constants, and local types into a utility pair
first; extract a cohesive stateful responsibility into a new class second; split
a static-method struct's implementations last — and applying it to the
then-current file is what makes the boundary defensible. The reviewer who raised
the observation saw no split in the changed regions, so a boundary written now
from that reviewer's evidence would be an unfounded guess.

Recommended constraint on whatever boundary is chosen: it is a pure relocation.
Function bodies, comments, log lines, assert text, validation order, thread
priorities, memory-ordering operations, and the pool layout computation move
verbatim; only namespace placement, include lines, and the declarations that
must gain external linkage change. Do not change what any moved code computes,
validates, publishes, or logs, and do not alter the eager/lazy split, the pack
integrity token computation, the chunk table order, any pack or manifest format,
or any chunk state transition.

Any new `.cpp` or `.h` created by the move is added to the client and server
Visual Studio projects and filters through `/update-vcxproj`, matching the
membership `PackChunks.cpp` already has. If the move relocates prose-cited code,
`Engine/Source/File/AGENTS.md` is repointed through `/update-claude-docs`.

## Critical files

- `Engine/Source/File/PackChunks.cpp` — the oversized file; 12,101/10,000 bt-token-v1 at this Plan's baseline.
- `Engine/Source/File/PackChunks.h` — the declaration counterpart, 1,613 bt-token-v1, under its 5,000 threshold; any boundary must keep it under it.
- `Engine/Source/File/FileManager.cpp` and `FileManager.h` — the sole owner of `PackChunks`; the only consumer whose includes a move can disturb.
- `Engine/Source/File/AGENTS.md` — `## Packed Assets` and `## Lazy-Pool Invariants` state the contracts the move must preserve, and cite code by responsibility.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/*.vcxproj` and their `.filters` — project membership for any new file.

## In scope

- Running `/reduce-file` on `Engine/Source/File/PackChunks.cpp` in standalone analysis mode against the then-current file and recording the boundary it selects.
- Creating the file or files that boundary names, holding the verbatim moved code, and deleting exactly that code from `PackChunks.cpp`.
- Adjusting only the `#include` lines the affected translation units then need, and adding declarations for moved symbols that acquire external linkage.
- Adding any new file to the client and server projects and filters via `/update-vcxproj`.
- Repointing any `Engine/Source/File/AGENTS.md` sentence that locates moved code, via `/update-claude-docs`.

## Out of scope

- Any change to what the moved code computes, validates, rejects, asserts, publishes, or logs, including error and log text, validation order, memory ordering, thread priorities, and pool sizing arithmetic.
- Any change to the eager/lazy type split, the pack integrity token or the chunk table order it hashes, the `.pack`/`.manifest` format or its versions, chunk state transitions, or the lazy-pool pointer/size contract.
- Behavior fixes to `PackChunks` owned by other live Plans, including the recommit failure path, duplicate CRC rejection, eager read completion, allocation validation, and chunk type flag validation.
- Reducing any other file, further decomposition once `PackChunks.cpp` is under threshold, and renaming, restyling, or rewriting comments in the moved code.
- Unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 1 (mechanical). Trigger: behavior-preserving code
relocation plus project membership, with no public signature, determinism/CRC,
serialization, wire, save/replay, threading, or trust-boundary change — the code
moves as written and every contract stays where `Engine/Source/File/AGENTS.md`
states it. If the boundary `/reduce-file` selects would change a public
signature, the client/server affinity of moved code, or any of those surfaces,
the implementing session reclassifies the change at the tier that surface
triggers before implementing.

Preserve these invariants:

- Observable behavior of every moved function is unchanged: same inputs, same
  published chunk state, same validation order, same assert and log text.
- The pack integrity token computed at startup is identical before and after, so
  existing clients and servers still handshake.
- Loader thread count, priorities, and the release/acquire publication order of
  chunk state are unchanged.
- Each lazy chunk's pool pointer and size are still fixed once at construction.
- New file membership matches the client and server projects that already own
  `PackChunks.cpp`, with filters entries matching the source folder.

## Acceptance criteria

- `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'Engine/Source/File/PackChunks.cpp'"` reports at or under 10,000 bt-token-v1, and every file created by the move is at or under its own threshold (10,000 for `.cpp`, 5,000 for `.h`).
- An exact diff review shows the moved bodies, comments, and literal strings are identical to their pre-move text apart from namespace placement, include lines, and added declarations.
- Client and server both build through `/compile`, and `/update-vcxproj` reports any new file correctly placed with no other membership drift.
- A client launched through `/agent-harness` connects to a server built from the same tree, proving the pack integrity token is unchanged, and loads island and texture chunks without a pack assert.
- No unit tests are added.

## Coordination

These live Plans edit regions of `PackChunks.cpp` without changing its length:
`Documents/Plans/Engine/ChunkRecommitFailureHandling.md`,
`Documents/Plans/Engine/DuplicateChunkCrcRejection.md`,
`Documents/Plans/Engine/EagerPackReadCompletion.md`,
`Documents/Plans/Engine/LazyPoolAllocationValidation.md`,
`Documents/Plans/Engine/PackChunkTypeFlagValidation.md`,
`Documents/Plans/Engine/AudioStreamingBackgroundRangeReads.md`, and
`Documents/Plans/Engine/TerminateHandlerPerThreadInstall.md`; five more cite it
read-only. This Plan changes no behavior, so it constrains none of them in
either direction and declares no dependency. Whichever lands second rebases and
re-resolves line numbers; if this Plan lands first, the others apply their edits
to whichever file now holds their target code.

## Notes

Origin: a `/repo-code-review` size observation recorded as a pre-existing
out-of-scope residual at baseline
`9c91258e550f958fe529dd9b48db8afd56865755` — "`Engine/Source/File/PackChunks.cpp`
is 12,051/10,000 bt-token-v1 ... the changed regions expose no cohesive split".
The reviewing session's other measured files were all under threshold
(`FileManager.cpp` 3,378; `Graphics.cpp` 7,704; `TextureManager.cpp` 9,538).

That session recorded the change as net-negative on `PackChunks.cpp`; the
measurements above show the opposite — the file grew by about 537 bt-token-v1 —
which changes nothing about the overage being pre-existing, but the implementing
session should trust the fresh measurement rather than that note.
