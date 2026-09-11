<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T22:12:18.374Z","dependsOn":[]} -->
# Delete the untracked-by-code `.GLTF_MODEL` scene leftovers

## Context

Four files with the extension `.GLTF_MODEL` are tracked beside the four scene
`.gltf` sources:

- `Engine/Data/Models/DualGeodesicIcosahedron/DualGeodesicIcosahedron.gltf.GLTF_MODEL`
- `Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf.GLTF_MODEL`
- `Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/scene.gltf.GLTF_MODEL`
- `Projects/BrokenEngineSandbox/Data/Models/spaceship2/scene.gltf.GLTF_MODEL`

No current code produces, consumes, or claims them. `ExportModel::Handles`
claims only the `.MODEL` extension
(`DataPacker/Source/ExportJobs/ExportModel.cpp:12-15`), and `ExportScene::Handles`
claims only `.gltf` outside `Intermediates`
(`DataPacker/Source/ExportJobs/ExportScene.cpp:9-24`); no other export job's
`Handles()` names the extension. A repository-wide search for `GLTF_MODEL`
excluding `ThirdParty/` returns exactly one textual hit across C++, scripts,
project files, and documentation: the `*.GLTF_MODEL          binary` line at
`.gitattributes:26`, which names that extension alone. Each file has the same
byte size as the current `.MODEL` sibling in its directory but different
contents and an older modification time, so each is an earlier generation of
the scene model output left behind when the generated extension became
`.MODEL`. Together they add about 8.8 MB of dead tracked bytes.

The finding was reported as a pre-existing residual of the
`Documents/Plans/Engine/SceneGeneratedOutputOwnership.md` change, which makes
the scene `.PreExport` marker an output-freshness record over the generated
`.MODEL` and block-compressed texture intermediates. Because nothing produces
or claims `.GLTF_MODEL`, these files are outside that marker's generated-output
set by fact and outside that Plan's scope, so removing them is separate debt.

## Design

Delete the four tracked files and the `.gitattributes` line that names only the
`*.GLTF_MODEL` extension, leaving the surrounding intermediate-asset attribute
lines for extensions that are still produced untouched. No code, build, or
attribute change beyond that single line is needed, because the search above
shows no producer or consumer to update.

The author recommends deleting the `.gitattributes` line together with the
files rather than leaving it: it is the only remaining reference to the
extension, and keeping it would preserve the impression that the pipeline still
emits that output.

## Critical files

- `Engine/Data/Models/DualGeodesicIcosahedron/DualGeodesicIcosahedron.gltf.GLTF_MODEL` — delete.
- `Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf.GLTF_MODEL` — delete.
- `Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/scene.gltf.GLTF_MODEL` — delete.
- `Projects/BrokenEngineSandbox/Data/Models/spaceship2/scene.gltf.GLTF_MODEL` — delete.
- `.gitattributes:26` — remove the `*.GLTF_MODEL` binary attribute line.

## In scope

- Deleting exactly the four tracked `.GLTF_MODEL` files listed under
  `## Critical files`.
- Removing the single `.gitattributes` line that names only `*.GLTF_MODEL`.

## Out of scope

- Any other generated sidecar extension, including `.MODEL`, `.PreExport`, and
  the block-compressed texture intermediates, and their `.gitattributes` lines.
- `ExportScene`, `ExportModel`, any other export job, `.pack`/`.manifest`
  formats, and the `.PreExport` marker contents owned by
  `Documents/Plans/Engine/SceneGeneratedOutputOwnership.md`.
- Scene `.gltf` sources, `.bin` buffers, source textures, and licenses in the
  same directories.
- Re-exporting or regenerating any asset, and any sparse-checkout or runtime
  data-mode change.

## Risk tier and invariants

Expected future implementation tier: Tier 1. Trigger: deleting tracked files
that no code produces or consumes plus one attribute line, with no public
signature, format, or invariant exposure.

Preserve these invariants:

- Every remaining generated sidecar beside each scene — `.MODEL`,
  `.PreExport`, and the block-compressed texture intermediates — stays
  byte-identical.
- DataPacker's export discovery and chunk set are unchanged, so no `.pack` or
  `.manifest` content changes.

## Acceptance criteria

- `git ls-files "*.GLTF_MODEL"` returns nothing, and `.gitattributes` contains
  no `GLTF_MODEL` reference.
- A warm DataPacker run after the deletions exports nothing, and `git status`
  afterwards shows only the four deletions plus the `.gitattributes` edit.

## Notes

The implementation session needs the island and texture data trees present to
run DataPacker warm, per `.agents/skills/compile/references/runtime-data-mode.md`.
