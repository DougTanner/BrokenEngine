<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:27.979Z","dependsOn":[]} -->
# Validate live cubemap face dimensions before concatenation

## Context

Final survivor `S002-C014` is a retained HIGH cubemap-shape finding. `ProcessLiveCubemap` loads six selected faces, overwrites width/height for each, and appends each payload without comparing dimensions (`DataPacker/Source/ExportJobs/ExportTexture.cpp:160-190`). The final header uses the sixth face's dimensions while runtime computes every layer offset from that one shape. A mixed-dimension face set can therefore shift layer boundaries or leave bytes unused.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C014 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:221` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:101`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to record the dimensions of the first selected face and require every later face to match before appending or publishing the combined payload. Route a mismatch through the existing texture export failure aggregate; keep current face order, one-mip live-cubemap behavior, and valid output byte compatibility.

## Critical files

- `DataPacker/Source/ExportJobs/ExportTexture.cpp:160-190` — live six-face loader/writer.
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — per-face texture shape/decode behavior.
- `Engine/Source/File/PackChunks.cpp` — runtime chunk range consumer (read-only contract evidence).
- `DataPacker/Source/ExportJobs/Texture/AGENTS.md` — cubemap ordering/shape contract.

## In scope

- Cross-face width/height equality validation before live-cubemap payload concatenation.
- Existing export aggregate failure handling for a mismatched face set.
- Valid six-face selection, ordering, encoding, and runtime layer shape.

## Out of scope

- KTX cubemap classification, per-face decoder algorithms, mip generation, runtime upload redesign, or shader changes.
- Face-name conventions, payload layout/version, or compatibility handling.
- New unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker texture export behavior). Trigger: opaque face dimensions determine an existing serialized cubemap payload and runtime layer arithmetic, while the fix is a single producer-side shape check with unchanged valid output.

Preserve these invariants:

- All six live-cubemap faces have one common width and height before concatenation.
- A mismatched face fails before a combined header/payload is published.
- Valid face order, mip count, encoding, and runtime layer boundaries remain unchanged.

## Acceptance criteria

- A six-face set with one differing width or height fails through the existing aggregate and publishes no combined live-cubemap output.
- A valid six-face set produces byte-compatible output and six correctly addressed runtime layers.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S002-C014`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C014 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:221`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:101`. No exact existing Plan was found. No source fix or build was performed during routing.
