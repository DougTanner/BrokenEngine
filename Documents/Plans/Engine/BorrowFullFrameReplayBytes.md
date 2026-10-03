<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:53:32.627Z","dependsOn":[]} -->
# Borrow Full-Frame Replay Bytes When Saving

## Context

`Engine/Source/File/DifferenceStream.h:173`, `DifferenceStreamWriter::Save`, inserts `mFullFramesStream.str()` into the `.fullframes` output. The constructor and `Update` append complete snapshots to that member (lines 45–62), so saving materializes an owning copy of the entire accumulated recording solely for immediate output. The accepted improvement is to eliminate this unnecessary owning intermediate.

`Projects/BrokenEngineSandbox/Source/Pch.h:42,59,76` enables `kbReplayFullFrames` in Debug and disables it in Profile and Release. This reduces Debug replay-save allocation/copy work; it makes no production frame-time or measured speedup claim.

## Design

Recommendation: replace exactly `rFullFramesStream << mFullFramesStream.str();` with `rFullFramesStream << mFullFramesStream.view();` in the existing atomic-write callback. Borrowing expresses the immediate consumer's need and retains the owning member buffer.

`FileManager::WriteFileAtomically` (`Engine/Source/File/FileManager.h:327–354`) calls the callback synchronously before closing and committing the output. The expression neither mutates the source buffer nor lets the view escape. The callback parameter is `std::fstream&`; the right operand changes from `std::string` to `std::string_view`. These standard-library operand types make the corresponding standard `operator<<` overloads available through argument-dependent lookup. The binary-stream trait only controls `WriteVersionedFile` and `ReadVersionedFile`; neither is called by this save path.

The standard specifies the lvalue string-buffer `str()` getter by construction from `view()`; the view refers to the initialized output range. The stream accessor forwards to its buffer. This removes the owning intermediate without removing bytes, including embedded nulls. Sources: [string-buffer members](https://eel.is/c++draft/stringbuf.members), [stringstream members](https://eel.is/c++draft/stringstream.members).

String insertion is specified through string-view insertion. Its counted output, padding, width reset, and stream-error behavior remain applicable. Sources: [string insertion](https://eel.is/c++draft/string.io), [string-view insertion](https://eel.is/c++draft/string.view.io).

## Critical files

- `Engine/Source/File/DifferenceStream.h`: `DifferenceStreamWriter::Save`, the `.fullframes` insertion only.
- Read-only evidence: `Engine/Source/File/FileManager.h`, synchronous `WriteFileAtomically` callback and its stream type; `Projects/BrokenEngineSandbox/Source/Pch.h`, build-configuration gates; `Engine/Source/File/AGENTS.md`, `## File Contracts` and `## Replay Streams`.

## In scope

- The single accessor replacement in `DifferenceStreamWriter::Save`'s full-frame output callback.
- Verification of output equivalence, retained ownership, and removal of the owning temporary.

## Out of scope

- Moving from or resetting `mFullFramesStream`, changing its type, storing a view, adding helpers, or changing callback lifetime.
- Other `.str()` sites, including DataPacker generated headers, timestamp formatting, client decompression, and reader buffer-reset setters.
- Stream interfaces, atomic publication and failure cleanup, snapshots, replay algorithms, simulation/CRC, threading, wire/save/replay formats, version constants, or configuration gates.
- Unit tests, benchmark infrastructure, new harness commands, comments, AGENTS.md edits, and style-guide policy changes.

## Risk and invariants

Tier 1: local behavior-preserving expression replacement with no public signature or invariant change. Replay proximity does not change the serialized range, format, or compatibility. Any change to those surfaces exceeds this Plan.

Preserve the complete initialized byte sequence and its ordering, member ownership and contents after saving, existing atomic-write success/failure handling, replay manifest publication, and replay checksums. No format/version bump is warranted. Performance non-regression follows structurally: the same counted output is performed while the recording-sized temporary allocation/copy is removed; no extra pass or retained owner is introduced.

## Acceptance criteria

- The source diff changes only the specified `.str()` getter to `.view()`.
- The standard insertion and synchronous lifetime argument still hold in the implementation toolchain; the writer retains its buffer and existing failure handling.
- The active Debug client and server build, and an existing harness-driven recording/save/playback exercise succeeds with full-frame output present and no new replay checksum mismatch.
- Full-frame output from an identical controlled baseline and changed recording has equal byte count and bytes, including binary zero bytes. Preserve the baseline artifact before repeating the scenario; use the existing harness's deterministic setup and tick controls. Do not compare unrelated recordings.
- Evidence establishes elimination of the owning `str()` intermediate; no timing threshold or wall-clock improvement is required.

## Verification

Use `/compile` for both BrokenEngineSandbox client and server `Debug|x64` with Shared data mode, as required for the planned harness scenario, so `kbReplayFullFrames` instantiates the changed branch. Use `/agent-harness` for baseline and changed recording/save/playback operations and controlled scenario setup; compare the resulting `.fullframes` artifacts outside the running game. Inspect the source and active standard-library implementation to confirm counted insertion and lack of the owning temporary. Record actual build, output-comparison, and playback evidence during implementation; none has been performed while authoring this Plan.

Apply the normal C++ code-style and comment reviews and `/update-claude-docs` synchronization check during implementation. `Documents/C++StyleGuide.txt` rule 40 governs borrowed text parameters; this expression adds no parameter and warrants no rule change. The File Contracts and Replay Streams constraints remain accurate, so the documentation check should produce no edits.

## Notes

No prerequisites or mandatory coordination were identified. Searches across live Plans for the target symbol, path, full-frame output, stringstream access, and snapshot-copy outcome found no duplicate. `Documents/Plans/Engine/ReplayInventoryRangesEqual.md` changes a distinct inventory comparison in `Replay.cpp`; neither Plan requires the other or changes the other's owned expression.
