<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:32:22.638Z","dependsOn":[]} -->
# Fix: Report raw render-target dump write failures

## Context

The accepted survivor `CAI/shard-0027/005` shows that
`EncodeAndWriteDump` opens, writes, and closes a raw `.bin` stream, then adds
the path to the success JSON without checking any stream state
(`Engine/Source/Graphics/Screenshot.cpp:396-441`).  An explicit agent path is
accepted by `ValidateDumpRenderTargetRequest`; Wind uses `R16G16_SFLOAT`, for
which raw output is the only encoding branch.  A missing or unwritable parent
therefore produces a false-success deferred response.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0027.md:131`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:807`.
All 14 frozen target rows matched and the PNG/token paths were separately
traced clean.  The defect is pre-existing at frozen commit
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source was changed in this
routing session.

Impact: automation receives a path for a missing or incomplete capture and
cannot distinguish that failure from usable render evidence.

## Design

Author's recommendation: treat the raw stream as a trust-boundary operation.
Require successful open, complete write, and successful close/state before
adding `raw` to the result.  On any failure call the existing
`ReportCaptureFailure` path and return without publishing a success result;
leave PNG handling and the existing deferred token contract unchanged.

## Critical files

- `Engine/Source/Graphics/Screenshot.cpp:34-43` — existing capture failure
  publication.
- `Engine/Source/Graphics/Screenshot.cpp:396-441,501-516` — raw writer and
  final result publication.
- `Engine/Source/Graphics/Screenshot.cpp:521-537` — request validation and
  raw-only format admission.
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:628-655,178-186` —
  explicit-path input and deferred response interpretation.

## In scope

- Checking open, write, and post-close stream state for the raw `.bin` branch.
- Publishing an error through `ReportCaptureFailure` and omitting the raw path
  whenever the stream does not commit the requested bytes.
- Keeping the current file naming, raw byte representation, PNG branches,
  capture token, and default-directory behavior.

## Out of scope

- Creating or validating arbitrary caller directories before the writer,
  changing path policy, PNG encoder behavior, or readback/fence ordering.
- A new capture protocol, result schema, retry queue, or partial-file cleanup
  policy beyond preventing a success result.
- Shader/readback formats unrelated to the raw writer.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: the change
handles opaque OS/file output and an agent-facing deferred result across
graphics and tool command boundaries.

Preserve these invariants:

- A result containing `raw` implies the file was opened, fully written, and
  closed successfully.
- Any raw stream failure resolves the active capture token with an error and
  never reports a success path.
- Valid Wind and other raw-capable formats retain the exact byte count and
  existing filename extension; PNG and normal capture behavior remain intact.
- No simulation CRC, replay, wire, save, or `.pack` data changes.

Tier rationale: the fix is confined to one writer function, checking stream
state and calling the existing `ReportCaptureFailure` path, which the Design
already specifies in full. Successful captures keep their exact bytes, filename,
and result schema, so only the already-broken failure case changes.

## Acceptance criteria

- `dump_render_target` for `WindOne` with an explicit missing/unwritable parent
  returns a deferred error and no success `raw` field.
- The same request with a writable path creates a `.bin` whose size equals the
  readback byte count and returns that path only after close succeeds.
- A PNG-capable target still returns its PNG result and reports encoder errors
  through the existing path.
- Client Debug and Release builds pass `/compile`; an `/agent-harness` capture
  scenario sees no false-success result or stuck token.

## Notes

The finding is about output publication, not whether the caller is allowed to
choose a path.  Keep validation and path-policy ownership in their existing
agent-command layer.
