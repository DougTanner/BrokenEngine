<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:47.526Z","dependsOn":[]} -->
# Pack Chunk Read Buffer Ownership

## Context

The all-rule lint review confirmed 2 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

The two `read-buffer-raii` rows describe one resource in `PackChunks.h:139`. Use
`common::AlignedUniquePtr<std::byte>` array elements and existing deleter (`Common/AlignedMemory.h:6`). Adopt
existing explicitly sector-aligned `_aligned_malloc` results with reset, preserve allocation size/failure
response; use get() at read/copy consumers (`PackChunks.cpp:909`, `:918`). Keep one buffer per loader and
unchanged bounds/indexing. At the original free-loop location (`:80`), reset owners after `mLoader.Stop()`
(`:69`), preserving release timing relative to the rest of teardown. Do not use MakeAligned's fixed alignment,
change handles, scratch storage or loader threading.

## Critical files

- `Engine/Source/File/PackChunks.cpp` — only the named candidate regions in candidates.json.

- `Common/AlignedMemory.h` — matching definitions or directly dependent contract.
- `Engine/Source/File/PackChunks.h` — matching definitions or directly dependent contract.

## In scope

Implement exactly 2 saved candidate rows in `PackChunkReadBufferOwnership/candidates.json` and their
correctness-dependent declarations/consumers. The ledger preserves original identity, source text and exact
wrapper definitions; this Design overrides historical alternatives or stale action labels. Do not repeat
completed formatter return-type, namespace or SAL edits.

## Out of scope

All candidates outside the saved ledger; new scanner rules or suppression;
filesystem policy and publication-container decisions; Wrapper constructors,
registrars, defaults/ranges, project membership or removal of emptied files;
new helpers/configuration; wire/save/replay/.pack formats and versions; frame
or Collection layout, CRC computation or trust changes; ThirdParty edits;
new unit tests, harness APIs, speculative cleanup or performance claims.
An unexpected behavior change returns for re-planning instead of widening scope.

## Acceptance criteria

- Every saved row is implemented once, using the exact symbol/type/initializer,
  order, guard and lifetime contracts in Design; no rejected row is changed.
- Before editing record current-tree build identity and a Debug /agent-harness sandbox
  connected-render/server-tick baseline. In both baseline and matching post-change run send audio_resume,
  audio_streaming_harness_rig clear_history, play with an available generated Data/Audio.h kAudioMusic*Crc,
  and bounded inspect calls until refill_ready appears for that generation and selected CRC, before clean
  shutdown. Retain command results and refill_ready records. Missing audio device/voice/music prerequisites or
  bounded runs without refill_ready block acceptance; boot/render or replay cannot substitute. Supplied
  evidence records no executed runtime scenario. Static source proof independently verifies sector alignment,
  allocation/failure/bounds/indexing and Stop-before-reset at the original free-loop position. Build sandbox
  client/server and ModelViewer Debug|x64 and Release|x64 through /compile.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

## Notes

Future implementation is Change Workflow Tier 3: loader-thread resource ownership and stop-before-release
threading invariant.

The user explicitly deferred all remaining implementation while preparing the
completed cleanup to land. This Plan records wanted debt, remains unclaimed,
and supplies no new implementation approval. Claim and obtain normal Change
Workflow approval before edits; reuse earlier analysis only after checking it
against the then-current tree. Tier 2+ needs plan audit, applicable simplicity
review, and Tier 3 needs grill and adversarial review. Apply triggered C++/GLSL
propagation, owner-document synchronization, style, correctness, comments,
over-engineering and independent acceptance verification; finalize verified
work through /finalize-changes. No unit tests or new harness commands.

Baseline evidence is static source at ad5d456751c76bf43c86b7cc5bd038946c81b1c0
plus the saved current candidate forms. Original coordinates may drift: match
path, symbol and text. A recorded runtime baseline must identify its actual
current-tree build; it is not an unedited-session baseline. All live operations
route through /agent-harness and all builds through /compile. Keep each live
run bounded; a proposed individual run over ten minutes requires its purpose,
estimate and user permission before launch. No runtime performance claim.

## Coordination

No directional prerequisites: this Plan can build and verify independently.
No mandatory nondirectional coordination: no sibling lint Plan owns these candidate files. Other
initialization Plans must independently preserve the existing library-phase logging lifetime.
Share matching
runtime evidence only when it settles each Plan's own acceptance criteria.
The four existing filesystem/publication Plans own separate deferred choices
and need no dependency or new reciprocal constraint from this Plan.
