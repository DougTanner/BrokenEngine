# DataPacker Export Jobs

Asset processors convert source files into cached `.pack` chunks. The parent `RunExportJobs<T>` orchestration discovers each job by `Handles()` and assembles its output.

## Shared Pipeline

- `ExportJob` owns content-fingerprint dirty checks, cache metadata, chunk I/O, and failure cleanup. `%LOCALAPPDATA%/BrokenEngine/DataPackerCache/<project>` holds chunks and fingerprints; a job's chunk, fingerprint metadata, and shader dependency sidecar names carry that job's export-format version. Checkout-local packs and manifests can be rebuilt from clean shared chunks without re-exporting sources.
- Before a clean cached chunk body is republished, its chunk-header identity (magic, the job's path CRC, and the NUL-terminated path text) and body extent are validated; a mismatch logs a warning and falls back to a full re-export.
- A successful export writes derived metadata before the primary fingerprint. Interrupted or failed work therefore remains dirty, and jobs remove incomplete sidecars in `CleanupOnFailure()`.
- `ExportJob::Version(N)` folds in `sizeof(common::ChunkHeader)`. Jobs that serialize additional payload structs also fold in their sizes (`ExportScene.h` and `ExportModel.h` show the pattern). Same-size reorder or semantic changes need the owning manual version bump; the layout `static_assert`s in `Common/DataFile.h` name which job's version owns each payload struct, so read the failing assertion message instead of guessing which version to bump.
- A job may version an expensive sub-stage independently of its chunk payload
  when that stage's outputs are tracked in the checkout, giving the stage its
  own marker and a fingerprint covering only its own inputs. Name and key that
  marker by the stage's own version alone, keep that version outside
  `Version(...)`, and bump whichever version owns the behavior that changed.
  Island texture encoding uses this pattern; the scene `.PreExport` marker
  also guards tracked outputs but departs from all three: its filename carries
  no version, its version is the scene's own `Version(...)` export version, and
  it fingerprints the stage's generated outputs instead of its inputs. Every
  such marker goes through `ExportJob::ReadMarkerFile`/`WriteMarkerFile` and is
  removed before the first output write.
- Each job's `std::async` launch wraps `RunExport` in `common::ThreadLocal::Entry`, so the job runs with its own worker's `ThreadLocal` and workbuffer. Keep job output and scratch isolated from other parallel exports.
- `AllocateHeaderAndData` is normally called once per export. Scene animation data is appended afterward; its chunk size excludes that section and a header pointer captured before vector growth is invalid after reallocation.
- `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` must fail before dirty Gaea, IBL convolution, or texture encoding begins. Clean cached outputs remain readable under this guard.

## Matching and Routing

`Handles()` determines both ownership and initial chunk flags. Filename tags and path components are inputs the output actually depends on: `[C]` marks cubemaps, `[BC4]`/`[BC5]`/`[BC7]` select block formats, and a `Raw` component marks raw assets. Shader stage flags and compression flags are added by their owning jobs. Scene discovery ignores `Intermediates` paths so generated Gaea meshes do not become standalone scenes.

Regular images generate a full mip chain; raw BCn/R16 and half-float intermediates preserve their supplied mip/face layout; KTX and live six-face cubemaps preserve cubemap ordering. Before encoding each live-cubemap face, the exporter requires the first face to be square and later faces to match its dimensions. Every final texture chunk is LZ4-compressed. Regular-path BC5 textures also publish the per-mip slope-variance data consumed by water shading; raw and cubemap paths do not synthesize it.

Texture encoding and chunk routing are described in `Texture/AGENTS.md`. Island/Gaea ingest is described in `Island/AGENTS.md`.

## Scene and Model

Scene discovery accepts `.blend`, `.gltf`, `.glb`, `.fbx`, `.obj`, `.dae`,
`.ply`, `.stl`, and `.3ds` outside `Intermediates`. A `.blend`, `.fbx` or
`.obj` file, its extension compared ignoring case, is an authored scene's
Blender source. A Blender source owns the `.glb` named after its full file name
(`X.fbx.glb`, `X.obj.glb`, `X.blend.glb`), suppressing that file's discovery
even when it already exists; an `X.glb` beside an `X.fbx`, `X.obj` or `X.blend`
is an ordinary export-only scene. The job uses the GLB path for
chunk identity, CRCs, manifest entries, generated symbols, and model/texture
names, including when the GLB is initially missing. Ordinary export-only scenes
need no Blender.
Commit available production Blender sources together with their export-ready
converted GLBs; none of these extensions is ignored. Conversion never saves the
author source, and DataPacker never performs Git operations.

Authored scenes convert before the ordinary two-phase scene export. Only
installed Blender 5.2 LTS is supported, selected from Windows installation
directories or `PATH`; no installer or converter configuration is provided.
Freshness checks inspect executable version resources without launching
Blender. Only dirty conversion launches it, within the scene's existing
asynchronous job; simultaneous dirty scenes retain separate processes and
scratch outputs. DataPacker's C++ code launches `blender.exe` through
`common::RunExecutable`; Blender executes the runtime input
`DataPacker/Scripts/ExportBlenderScene.py` with its bundled Python, outside
Visual Studio project membership. DataPacker does not launch `python.exe`,
require a separate Python installation, or embed Python. The script opens a
`.blend`, and imports an FBX or OBJ into an empty scene with Blender's bundled
importer. For an FBX source, the script overrides material alpha from the FBX
file's own transparency properties; `DataPacker/Source/AGENTS.md`
`## Architecture` owns when such an override is allowed. Before export, the
script drops animation actions that move nothing.

The authored scene's primary fingerprint hashes its Blender source, not the GLB
that conversion replaces after the fingerprint is captured. A separate shared
cache marker uses the converter's own version, independent of scene payload
and `.PreExport` versions. It records source, tool identity/content, script and
recipe, consumed external dependency hashes, and published GLB content.
Dependencies include transitive linked Blender data and unpacked images used
by exported content, excluding unrelated datablocks, plus each existing file an
OBJ's `mtllib` statements name, the `.mtl` file sharing the OBJ's stem (`X.mtl`
for `X.obj`) when it exists whether or not a statement names it, and every
candidate image in the source file's folder tree (extensions
`CANDIDATE_EXTENSIONS` in the script, mirrored by `kpCandidateImageExtensions`
in `BlenderExport.cpp`), from which conversion relinks missing images and fills
empty material inputs. In-root dependencies use input-root-relative
identities; external dependencies use source-relative identities where
possible. Missing or unreadable dependencies, tool or marker,
changed inputs or output, and a candidate image with no recorded dependency
force conversion before any clean return.

Conversion validates a private sibling GLB stage before replacing the output
on the same volume. Process or validation failure preserves the previous GLB
and fails that scene through the existing aggregate, preventing scene pack
publication while unrelated exports continue. The conversion marker is
invalidated before replacement and committed after successful publication;
an incomplete publication stays dirty. Later scene packing failure does not
invalidate a successful conversion, so repairs can reuse it. Dirty conversion
also forces normal pre-export from the newly published GLB.

`GltfSceneDecoder` owns glTF/GLB loading and decoding into `SceneExportData`,
which owns the consumed image, texture-slot, mesh, material, skeleton and
animation values without references into the source model. `ExportScene` owns
optimization, intermediate/chunk writes, and cache/publication behavior.
Material decoding preserves RGB emissive factors and glTF occlusion strength;
formats without an occlusion strength keep the neutral default.
A glTF primitive with no material gets the glTF 2.0 default material.
glTF decoding stays phased: source loading, canonical skin selection and normalization
precede pre-export marker removal; texture publication and orphan sweeping
precede pre-export skeleton/animation and mesh/material checks. Main-only export
retains its own consumed-data predicates and validation order. Blender staging
validation loads the source without decoding export payloads.

`AssimpSceneDecoder` decodes the four exchange formats directly into the same
owned payload, including representable skeletal and node TRS clips. Fidelity
starts at the decoded Assimp scene: importer conversions are accepted, but the
exporter rejects unsupported behavior instead of adding approximations. Morphs,
mesh animation and unsupported material behavior remain rejected, including
nonzero effective Phong specular contribution, wireframe and two-sided
rendering; inactive importer defaults are accepted. PLY alone ignores Assimp's
synthesized sidedness and imports one-sided. Mesh instances retain effective
material splits; omitted materials have concrete defaults.
Missing normals are generated; invalid supplied normals fail. Importer root
corrections precede conversion to engine Z-up/meters; Collada's corrected root
uses meters. Static vertices bake mesh-world transforms,
inverse-transpose normals and determinant-correct winding.

Assimp clips must fit the packed hierarchy, skin and animation capacities.
Animated nodes must decompose into TRS; keys must preserve their timing,
interpolation and reached boundary behavior in the runtime representation.
Animated scale must stay nonzero over the played interval; valid STEP changes
and reflections remain supported. One packed skin requires consistent
inverse-bind matrices for shared bones and at most four normalized nonzero
weights per vertex. Animated exports keep mesh-local vertices, source keys and
inverse binds, with separate unanimated ancestors for basis conversion and
default-pose normalization.

DAE and 3DS UVs flip V exactly once into the engine's top-origin image
convention. PLY UVs pass through unchanged despite its undefined format origin;
STL has no UVs. Absent UV channels stay zero.

Assimp scenes keep a source-only primary fingerprint and an independently
versioned scene dependency sidecar. Successful auxiliary opens and external image
reads record input-root-relative identities and content hashes, including an
empty dependency set. Warm checks read that sidecar without loading Assimp;
missing or changed dependencies force pre-export. Failed opens fail the asset
even if Assimp would substitute a default. Export failure removes both markers.

The ordinary two-phase export's `.PreExport` marker
covers the scene's generated
outputs — the `<scene>.MODEL` file and every matched texture intermediate — not
just the source scene, so a deleted, reverted, or edited output re-runs
pre-export. The main phase writes scene metadata and optional animation data.
Pre-export also removes orphaned scene texture intermediates so recursive
texture discovery cannot ship stale assets. Generated assets are referenced by
relative-path CRC. An export-only glTF scene's dirty check covers the source plus every external
file it references, so editing a referenced texture or buffer re-runs
pre-export; a reference the scene names but disk does not have keeps the scene
dirty, leaving that failure to the per-asset export rather than aborting the
whole run. Dependency enumeration parses text glTF JSON or the bounded first
JSON chunk of a GLB without loading binary buffers or decoding images. Invalid
GLB framing contributes no dependencies and leaves failure to the export-time
loader. The source fingerprint covers embedded bytes.

Scene texture pre-export deduplicates attempts by source and output format. Each attempt writes a private, untagged stage; all attempts must finish before any final intermediate is published. Keep cleanup ownership separate for stages from this attempt and final paths published by it, because a failure during encoding or publication must not classify one kind of path as the other.

A scene pack supports one node-referenced skin, and a scene whose nodes reference
more than one skin is rejected. A vertex joint index is bounded by that skin's
full joint count: a scene with a weighted joint index at or above that count, or
any joint index at or above it when the primitive carries no weights, is rejected;
a zero-weight slot is not checked. CPU and GPU consumers must skip zero-weight
slots before dereferencing their unused joint indices.

Exported default-pose geometry is centered at the origin with its longest
extent spanning -1..1, so game render scales are plain sizes. For glTF,
`GltfSceneDecoder` inserts one synthetic, unanimated root node
at index 0 holding only a translation and a uniform scale. Every default-scene
root becomes its child and every node reference shifts by one, so an exported
skeleton's node index is the source index plus one. Static exports bake node 0
into their vertices; skeleton exports ship it as the hierarchy root and keep
vertices mesh-local and inverse-bind matrices and keyframes as authored. Bounds
cover only drawn vertices at their runtime default-pose positions; no drawn
vertex, non-finite bounds, or a zero extent fails the export. A skin whose
joints lie outside the default scene does not inherit node 0, so its geometry
exports un-normalized.

When multiple mesh nodes reuse one source material, or primitives under one material differ in deformation mode (skinned versus not), preserve distinct material entries while retaining the source material index used for texture lookup, so no draw mixes deformation modes.

Vertex colors are not part of the packed model format. Scene export warns with
source path and primitive identity when `COLOR_0` is present and ignores that attribute
without modifying the source.

## Cubemap Pre-pass

Irradiance and prefiltered cubemaps are generated before ordinary export jobs.
Their cache fingerprints cover the source KTX or all six faces, and incomplete
writes remain dirty. A pass contains each input's failure, reports one aggregate
diagnostic, and returns failure without aborting the run, so the other pass and
all later work still run. Each pass then removes its own intermediates that
current inputs no longer expect, so texture discovery cannot ship a deleted or
renamed source's stale chunk; a pass whose input discovery fails skips that
removal. A mirrored cache sidecar is the only proof of ownership, so a pass
never removes another producer's files. Prefiltered radiance is ordered by face
first, then by mip level within each face, to match runtime upload order. The
resulting half-float intermediates are consumed through texture raw routing.

## Shader Dependencies

Shader export records every dependency from `glslc -MD` as an input-root index, root-relative path, and content fingerprint. Never persist absolute worktree paths. Missing or changed dependencies recompile the shader; Vulkan SDK header-version changes also invalidate shader chunks.

## Audio Policy

Audio export accepts only 16-bit PCM and 32-bit float `.wav` sources; anything
else asserts, so a 24-bit WAV cannot simply be dropped into the asset tree.
Output is interleaved 48 kHz PCM matching the runtime mastering rate —
`AudioRepair.h` owns that rate constant, kept in lockstep with the engine's
mastering voice, plus every repair policy toggle. The 16-bit decode divisor and
the rounded output conversion are a matched pair that makes a defect-free 16-bit
file round-trip bit-exactly; change them together or not at all. Repairs operate
only on packed output and run before resampling; source files are never
modified. `_loop` filename stems use seam validation instead of edge fades, and
a `Music` path component limits repair to safe transformations. Policy,
repair-order, or export-rate changes require an `ExportAudio` version bump.

## See Also

- `Island/AGENTS.md` - Gaea route caches, split lifecycle, and island payloads
- `Texture/AGENTS.md` - texture intermediates, encoding, migration, and chunk formats
- `../../../Common/AGENTS.md` - shared chunk and serialization contracts
