# Scene generated-output ownership and dirty-state validation

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0006/007` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`ExportScene::CheckDirty` decides that a scene is clean from the outer cache
state and `.PreExport` marker at
`DataPacker/Source/ExportJobs/ExportScene.cpp:27-49`. Its source fingerprint
at `ExportScene.cpp:232-246` covers the `.gltf` and referenced URI inputs but
does not establish that the generated `.MODEL` and block-compressed texture
outputs required by the scene still exist and match the scene's published CRC
references. The clean path then enters the main export, which reads generated
model/texture intermediates and writes scene model/texture CRCs
(`ExportScene.cpp:306-319,750-815`). `DataPacker/Source/Main.cpp:341-349`
discovers generated outputs as independent jobs later in the run.

With unchanged source hashes and marker/version, deletion or replacement of a
generated output can therefore leave the scene on the clean path while its
published scene data names an absent or stale producer output. Runtime model
pipeline and texture consumers resolve those references
(`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:35-43`;
`Engine/Source/Graphics/Objects/ModelPipeline.cpp:187-203`).

The durable source evidence above proves the current root cause and boundary;
the ignored shard report is supplementary provenance, not the sole proof.

## Controlling contract and invariant

`DataPacker/Source/AGENTS.md` requires opaque intermediates to remain valid
before use and requires every asset type to continue after earlier failures.
`DataPacker/Source/ExportJobs/AGENTS.md` defines the versioned `.PreExport`
marker as governing generated model and block-compressed texture intermediates,
requires scene dirty checks to cover referenced inputs, and requires generated
outputs to remain connected to their CRC consumers. The future correction must
keep the scene's generated producers and runtime CRC consumers in one
consistent ownership contract.

## Boundary and impact

The open boundary is the scene clean decision and the ownership relationship
between `.PreExport`, generated `.MODEL`/texture intermediates, scene CRC
references, and independent output discovery. It does not decide a runtime
model-pipeline fallback or alter unrelated cache-body validation.

If the output disappears or is stale while the marker and source fingerprint
remain clean, the DataPacker can publish a scene that references missing
assets; runtime startup or pipeline creation can then fail or assert.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Output-membership validation.** Extend the scene clean predicate with a
   deterministic required-output membership/identity check. Define which
   generated files and formats belong to each scene, how their expected CRCs
   are derived, and how missing or stale outputs mark the scene dirty while
   preserving failed-output cleanup.
2. **Canonical artifact ownership.** Make a canonical generated-artifact
   manifest or producer-owned record authoritative for scene publication and
   independent discovery, then define how `.PreExport`, scene CRC references,
   and output cleanup consume that ownership. This may change which producer
   is responsible for proving completeness.

The decision must not silently treat a missing generated output as a valid
clean scene or make runtime consumers accept an absent CRC.

## Decisive questions and acceptance evidence

- Which generated model and texture outputs are required for each scene
  variant, including optional animation/material paths, and which producer owns
  each path?
- Does the `.PreExport` marker represent only source freshness or also a
  complete output set? If it remains a freshness marker, where is the durable
  membership/identity record stored?
- With unchanged `.gltf`, URI, marker, and scene chunk, does deleting or
  replacing one required generated output force a dirty regeneration before
  scene CRC publication?
- Do valid complete outputs stay clean and byte-identical, while a failed
  regeneration preserves the prior published output and reaches later
  independent export types?
- Can runtime model/texture consumers resolve every CRC emitted by a valid
  scene without depending on a later independent discovery pass?

The eventual executable Plan must state the chosen ownership model, exact
scene/export regions, failure propagation, generated-output invariants, and
DataPacker/runtime acceptance scenario. Expected future work is Tier 3 because
the decision can cross offline publication, generated asset ownership, and
runtime pack consumers; the tier must be re-evaluated when the choice is made.

## Provenance

- Frozen source candidate: `CPT/shard-0006/007`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Existing `Documents/Plans/Engine/CacheChunkBodyValidation.md` was reviewed;
  it owns inner cached chunk-body validation, not this scene generated-output
  membership/ownership boundary. No exact existing record owns this finding.
- No source, asset, or scheduler change is part of this investigation.
