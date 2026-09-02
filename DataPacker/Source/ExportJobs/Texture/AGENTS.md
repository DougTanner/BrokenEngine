# Texture Export Helpers

Texture transforms, intermediate-file I/O, block encoding, and legacy-intermediate migration. The parent ExportJobs hub owns asset matching and final chunk routing.

## Intermediate Contract

`Texture::Save` writes a magic marker, dimensions, mip count, then the encoded payload. BCn and R16 intermediates are zlib-compressed on disk; half-float cubemap pre-pass output remains raw. Readers must use the shared parser and its returned payload offset rather than duplicating header math.

Texture writers use a private, untagged staging name and atomically replace the requested destination only after the complete file is flushed and closed, so an in-progress file cannot be picked up by texture routing. Scene pre-export keeps one staged output per unique source/format pair, completes every attempt before publishing any final intermediate, and tracks unpublished attempts separately from outputs published by the current attempt so failure cleanup keeps those ownership boundaries intact.

Final texture chunks use LZ4 and store compressed and uncompressed sizes for `FileManager`. Raw intermediate input is decoded as needed and re-encoded for the chunk; changing the intermediate or chunk contract requires coordinated producer, exporter-version, shared-header, and runtime-reader updates.

## Encoding

Mip generation, format conversion, underwater masking, and direct BC4/5/7 block compression are offline quality-sensitive work. Preserve deterministic inputs and the configured base encoders; the optional bc7e.ispc route remains disabled. The shared worker pool dispatches bounded block-row ranges, and whole encode chains remain serialized by the caller-held `sEncodeMutex`; [Common threading](../../../../Common/Threading/AGENTS.md) owns `Dispatch` participation and reentrancy. `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` fails before encoding in both the job entry point and shared encoding path.

## Migration

`MigrateLegacyIntermediates()` runs before readers consume cached intermediates. It walks the tracked asset input directories (`gpFileManager->mpInputDirectories`), not the `%LOCALAPPDATA%` cache, and rewrites qualifying files in place. It is idempotent: files already carrying the magic marker are skipped. Migration validates the legacy shape and payload before that rewrite; preserve the ordering, because the write is not transactional and it is changing tracked files. Dimensions, mip count, texture format, and payload meaning must survive migration.

Eligibility is deliberately narrow: the inverse format map recognizes only the BC4, BC5, BC7, and R16 intermediate extensions, and everything else is skipped. The IBL (image-based lighting) `.R16G16B16A16_SFLOAT` cubemap intermediates are excluded on purpose — they are legacy-shaped by design and carry no magic marker, so they look exactly like unmarked files awaiting migration. Widening the format map would zlib-wrap and re-header raw half-float cube faces and silently break IBL.

## See Also

- `../Island/AGENTS.md` - island texture producers and masking order
- `../../../../Engine/Source/File/AGENTS.md` - runtime chunk decompression
