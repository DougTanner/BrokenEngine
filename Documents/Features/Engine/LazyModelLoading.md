# Lazy Model Loading

Revisit When: model loading becomes a meaningful share of startup time, or the unit roster grows enough that not every model is needed per session.

## Context

Pack data types split into eager and lazy at runtime only, in `IsEagerChunk` (`Engine/Source/File/PackChunks.cpp`): Scene, Model, Shader, and Raw are eager; Audio, Islands, and Texture are lazy. Eager packs are read whole by the startup task in `PackChunks::LoadPackFiles`, and `PackChunks::GetEagerChunkMap` blocks until that task finishes. Lazy chunks are loaded on request by `PackChunkLoader` into the lazy pool, which is laid out once at construction over the whole lazy chunk map.

Model upload happens entirely at boot. The `BufferManager` constructor walks the eager map, validates each `ChunkFlags::kModel` chunk's `ModelHeader`, and creates one device-local index-plus-vertex `Buffer` per model in `BufferManager::mModelMap`, keyed by model CRC and filled through a one-shot staged copy. Consumers resolve that map while pipelines are created: `ResolveModelChunkShaders` (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp`) maps a scene's `modelCrc` to its buffer, the HexShields pipelines bind the DualGeodesicIcosahedron model directly, and `ModelPipeline::Create` validates material ranges against the buffer's index count. `ModelPipeline::RecordDrawIndirect` and `Pipeline::RecordDrawIndirect` bind the vertex buffer inside the Global and Main command buffers, which are recorded once (`Engine/Source/Graphics/AGENTS.md`), so the buffer handle is fixed into the recording.

Textures already load lazily without re-recording. `Pipeline::WriteIndirectBuffer` and `ModelPipeline::WriteIndirectBuffer` request a pipeline's texture chunks through `PackChunkLoader::RequestChunkLoad` on the first positive instance count. The loader hands `kTexture` chunks to `TextureUploadManager::RequestUpload`, and the texture stays on the white placeholder until adoption swaps its bindless descriptor. That upload path copies images only (block-compressed partial copies, image ownership transfer); other lazy chunk types stop at `kReady`. Island terrain meshes are the buffer precedent: `IslandTemplate` mesh data slices the lazy Islands chunk, lands in a stable mesh arena whose residency is published through indirect records rather than a re-record, and has its CPU slice released by `PackChunks::DecommitChunkRange` (`IslandTemplate::bMeshCpuDecommitted`).

The server never opens the Model pack: `IsServerChunk` accepts Islands only. DataPacker has no notion of eager or lazy, Model chunks are written uncompressed, and the lazy open path already validates any chunk type's location and header, so moving Model to the lazy set changes no `.pack` or `.manifest` layout and needs no `common::DataHeader::kiVersion` bump.

Four models are tracked today: the engine DualGeodesicIcosahedron and the sandbox Spaceship, spaceship2, and aim-9_missile.

## Design

Move Model from the eager set to the lazy set in `IsEagerChunk`, and load each model's index and vertex data on first use through the same request mechanism textures use: the first positive instance count in a pipeline's indirect write requests the model chunk, and its GPU buffer is filled once the chunk is ready. The old notes also suggested splitting data into a preload set, just enough for the title screen, and the rest.

Open choices:

- **Not-yet-resident model at draw time.** Skip the draw, writing a zero instance count to the indirect record until the buffer is filled; draw a placeholder mesh; or block the first use on `PackChunks::WaitForChunks`. The record-once command buffers mean any choice keeps a buffer handle that never changes, for example a device-local buffer created at boot from the lazy chunk header's `ModelHeader` sizes and filled later.
- **Upload path.** Extend `TextureUploadManager` to buffer copies, or fill model buffers with a one-shot staged copy on the main thread when the chunk is ready.
- **Preload set.** Which models, if any, still load at boot, such as the DualGeodesicIcosahedron the HexShields pipelines bind, or whatever the title screen draws, and how that set is expressed.
- **CPU copy after upload.** Keep the model's lazy-pool bytes resident, or release them as island meshes do and reload on device-loss recovery.

## Out of scope

- Server changes; the server does not load models.
- Compressing model chunks or changing their layout. If a later design does, it bumps the owning pack version, with no backward compatibility.
- Scene chunks, which stay eager: animation data aliases eager pack memory (`Engine/Source/Graphics/AGENTS.md`).
- Evicting models once loaded.

## Notes

- Low value today: with only four models, boot-time model upload has little to save, while the change touches record-once draws, the loader, and device-loss recovery.
- Client-only rendering work; no determinism or CRC exposure.
- File's rule that the eager/lazy split stays aligned with what DataPacker emits (`Engine/Source/File/AGENTS.md` `## Packed Assets`) holds without DataPacker changes, since DataPacker does not encode the split.
