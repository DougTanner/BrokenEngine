<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T17:19:04.213Z","dependsOn":[]} -->
# Report texture-cache atomic-write outcomes

## Context

`engine::TextureCache::SaveTextureToCache` (`Engine/Source/Graphics/Managers/TextureCache.cpp:252-279`) currently writes the cache header and GPU-readback data through `gpFileManager->WriteFileAtomically` at `:272-276`, then explicitly discards its `bool` result with `static_cast<void>` and unconditionally logs `"Saved texture cache ..."` at `:278`. The false required condition is that a cache-save success log must only be emitted after the atomic write commits. `FileManager::WriteFileAtomically` is a `[[nodiscard]] bool` whose contract says that open, stream/close, or rename failure returns `false` (`Engine/Source/File/FileManager.h:179-182`, implementation `:285-296`); `FileManager::CommitAtomicWrite` logs and returns `false` for a bad stream or failed rename (`Engine/Source/File/FileManager.cpp:333-359`).

This is an accepted pre-existing, out-of-scope residual found during the C++ review-finding resolution for `Documents/Plans/Graphics/VulkanPipelineCacheCorruptionGuard.md`. The originating Plan's active implementation is the Vulkan pipeline-cache CRC/framing/load guard in `DeviceManager` and `Pch`; its changed-file baseline `34f8d6fad82abf4af74e185d40e0e79066fd47aa` contains no change to `TextureCache.cpp`, `FileManager.h`, or `FileManager.cpp`. Texture-cache save reporting is a separate graphics-manager cache path and was deliberately not expanded into that change.

## Design

Capture the return value of the existing `WriteFileAtomically` call in `TextureCache::SaveTextureToCache` and emit the existing success log only when it is `true`. Preserve the current cache header/data writes, destination and flags, success message text, and helper-owned failure logging. Do not add a second failure log: the helper already reports the failing atomic stage and preserves the previous file on failure.

## Critical files

- `Engine/Source/Graphics/Managers/TextureCache.cpp` — `TextureCache::SaveTextureToCache` write and success-log region (`:252-279`)
- `Engine/Source/File/FileManager.h` — `FileManager::WriteFileAtomically` contract/reference only; unchanged
- `Engine/Source/File/FileManager.cpp` — `FileManager::CommitAtomicWrite` failure semantics/reference only; unchanged

## In scope

- The `TextureCache::SaveTextureToCache` `WriteFileAtomically` call and following success log: retain the existing serialized bytes and write arguments, consume the boolean result, and gate the one success log on a successful atomic commit.

## Out of scope

- `FileManager::WriteFileAtomically`, `FileManager::CommitAtomicWrite`, `FileManager::OpenFile`, and all other callers.
- Texture-cache header layout, versioning, source-CRC matching, load validation, GPU readback, and generated-texture behavior.
- `DeviceManager`, `Pch.h`, and any other work owned by `Documents/Plans/Graphics/VulkanPipelineCacheCorruptionGuard.md`.
- New logging formats, new cache recovery behavior, unit tests, or unrelated cache/save paths.

## Risk tier and invariants

Tier 2 — scoped client-graphics behavior: one cache-save caller changes only whether its success diagnostic reflects the helper's boolean result. The persisted texture-cache bytes and layout, CRC/determinism, wire/protocol, save/replay, threading, allocation, trust-boundary, and build/bootstrap contracts are unchanged.

Invariants:

- A `false` result from `WriteFileAtomically` emits no `"Saved texture cache ..."` success log; the helper's existing failure log remains the only failure report for that write.
- A `true` result emits the existing success log exactly once with the same path and message text.
- The header and texture data are written with the same order, flags, destination, and serialization as before.

## Acceptance criteria

- Through `/agent-harness`, launch the client with an isolated `--app-data-directory` and capture that run's client log. Before the first clean startup/shutdown, leave the client AppData child `BrdfLut.cache` absent and create only the exact `BrdfLut.cache.tmp` directory; require the default-visible `Loading/Error` line `WriteFileAtomically failed to open "BrdfLut.cache.tmp"` and, after shutdown, no `BrdfLut.cache` in that child. While the client is stopped, remove only that exact obstruction and rerun the same clean startup/shutdown; require no matching helper failure and `BrdfLut.cache` present afterward.
- The final diff shows the existing `LOG(kGraphics, kDebug, "Saved texture cache to {}", rCachePath.string())` call remains a single call inside the true branch of the `WriteFileAtomically` result, so a false result cannot reach it and a true result can.
- `WriteFileAtomically` and `CommitAtomicWrite` remain unchanged, and the saved texture-cache bytes remain identical for a successful write.
- The targeted client build passes through the repository `/compile` workflow.

## Notes

- Duplicate search before drafting covered live Plans for `TextureCache`, `WriteFileAtomically`, ignored results, unconditional success logging, atomic-write failure, cache-save outcome, and the originating scope boundary. Only `Documents/Plans/Graphics/VulkanPipelineCacheCorruptionGuard.md` mentions the pipeline-cache write helper; it does not own this TextureCache root cause or implementation boundary.
- No dependency or reciprocal `## Coordination` section is required: this is independently landable and does not modify the originating pipeline-cache Plan.
