<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-10T06:08:06.112Z","dependsOn":[]} -->
# Reconcile only owned scene texture intermediates

## Context

The Tier-3 adversarial review of the active texture publication change found a pre-existing scene-output reconciliation defect. `ExportScene::ProcessTextures` (`DataPacker/Source/ExportJobs/ExportScene.cpp:368-397` in the current checkout; `:274-304` at session baseline `0afc952c3e9a018e7bf78f9573e96c2d02544286`) builds the current write set, then removes every regular sibling whose name starts with `mInputPath.filename() + ".Texture"` and is not in that set. The predicate does not require the decimal source-image index or a recognized scene-intermediate suffix, so a file such as `Foo.gltf.TextureNotes.txt` can be deleted during a successful pre-export.

The current scene names are produced by `GetTextureIntermediatePath` from the scene filename, a nonnegative source-image index, and the `ComputeTextureFormats` formats (`.BC4_UNORM_BLOCK`, `.BC5_UNORM_BLOCK`, or `.BC7_UNORM_BLOCK`). The broad prefix predicate is unchanged from the session baseline; the active `TextureIntermediateAtomicPublish` Plan changes staging/publication and failure cleanup, not this reconciliation boundary. The live `IblIntermediateReconciliation` Plan is limited to IBL outputs and explicitly excludes generic reconciliation. No existing Plan owns this scene-specific root cause and boundary.

## Design

Keep the existing current-write-set comparison and post-success cleanup timing, but make an orphan candidate match exactly the scene-owned identity: the exact scene filename prefix, one or more decimal source-image-index digits, and one of the three suffixes emitted by `ComputeTextureFormats`. Compare the suffix against the canonical `TextureIntermediateSuffix` values for BC4, BC5, and BC7 so producer and reconciliation names cannot drift; do not broaden the scene sweep to the R16 or IBL-only suffixes. A candidate is removed only when that complete name is absent from the current write set. Preserve all other sibling files byte-for-byte, including names that share only the textual prefix or have an unrecognized suffix.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp`

## In scope

- `ExportScene::ProcessTextures`: the local orphan-candidate name predicate recognizing the exact scene texture-intermediate identity; preserve current write-set comparison, deletion timing, and logging. No public API or header change is needed.

## Out of scope

- `Texture::Save` or `ExportScene::ProcessTextures` staging, future draining, publication, and failure-cleanup changes owned by `Documents/Plans/DataPacker/TextureIntermediateAtomicPublish.md`.
- IBL cubemap reconciliation owned by `Documents/Plans/DataPacker/IblIntermediateReconciliation.md`, generic producer reconciliation, and any path outside the scene's sibling directory.
- Encoded texture bytes, chunk or manifest formats, CRC/version constants, migration behavior, and unrelated scene export behavior.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: changing the files admitted to the recursive texture exporter changes the `.pack`/`.manifest` producer chain. Invariants: only recognized scene texture intermediates can be removed; unrelated prefix siblings remain byte-identical; unchanged valid inputs retain byte-identical intermediates and export outputs; no serialization, version, or texture encoding contract changes.

## Acceptance criteria

- A successful pre-export removes a stale recognized scene intermediate whose complete name is absent from the current write set, including stale BC4, BC5, and BC7 variants.
- A successful pre-export leaves unrelated regular siblings byte-identical when they share the scene prefix but fail the exact scene-intermediate identity, including `Foo.gltf.TextureNotes.txt` and names with unrecognized suffixes or nonnumeric indices.
- A fixture or live DataPacker run containing both categories proves the stale recognized file is removed while the unrelated file remains unchanged, and a full export from unchanged inputs remains byte-identical.

## Coordination

This Plan is order-independent with `Documents/Plans/DataPacker/TextureIntermediateAtomicPublish.md`: both touch `ExportScene::ProcessTextures`, but this Plan owns only the post-publication orphan predicate while that Plan owns staging/publication and failure cleanup. Locate each region by symbol and preserve both boundaries when they land together. No directional dependency is required.

## Notes

Origin: Tier-3 adversarial review residual, confirmed against the session baseline and current source. No unit tests are added; use the DataPacker fixture or live verification path for the observable file-preservation checks.
