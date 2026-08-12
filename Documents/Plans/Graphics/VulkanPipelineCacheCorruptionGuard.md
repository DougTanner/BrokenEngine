<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T01:32:40.855Z","dependsOn":[]} -->
# Guard the Vulkan pipeline cache against corruption and re-enable it

## Context

`Projects/BrokenEngineSandbox/Source/Pch.h:18` disables the Vulkan pipeline cache with `inline constexpr bool kbVulkanPipelineCache = false;`, recording that the cache must not be re-enabled until a corruption guard exists. The load path in `Engine/Source/Graphics/Managers/DeviceManager.cpp:382-431` (`DeviceManager::LoadPipelineCache`) already validates the `VkPipelineCacheHeaderVersionOne` prologue against `vendorID`, `deviceID`, and `pipelineCacheUUID`, and discards a mismatch with a `kDebug` log. That check proves the blob was written by a compatible driver on this device; it proves nothing about the bytes after the 32-byte header. A truncated write, a bad sector, or a half-flushed shutdown produces a header-valid, payload-corrupt file, which is fed straight into `vkCreatePipelineCache` at `:429` and then into every `vkCreateGraphicsPipelines` call through `mVkPipelineCache` (`Engine/Source/Graphics/Objects/PipelineCreator.cpp:659`). Vulkan gives the implementation no obligation to detect that, so the failure mode is a driver-side crash or hang at boot with no diagnostic — which is why the switch is off.

The same load path has three unguarded I/O failures that already turn a bad file into a boot failure rather than a fallback. `gpFileManager->OpenFile` returns its `std::fstream` whether or not the open succeeded — it only logs `"Failed to open"` (`Engine/Source/File/FileManager.cpp:193-206`). `DeviceManager.cpp:397-402` then seeks and calls `tellg()` on a possibly failed stream, assigns the result to `int64_t iSize`, and passes it to `cacheData.resize(iSize)`. On a failed stream `tellg()` returns `-1`, so `resize` receives `size_t(-1)` and throws. The subsequent `read` is likewise unchecked; `resize` value-initializes, so a short read leaves the tail zero-filled rather than uninitialized, and a file whose complete header was read but whose payload was not still passes the header check and reaches Vulkan as a zero-padded, incomplete payload.

The save path is `DeviceManager::~DeviceManager` (`DeviceManager.cpp:337-363`), which writes the raw `vkGetPipelineCacheData` blob to `pipeline.cache` in the appdata directory through `gpFileManager->WriteFileAtomically`.

## Design

Write a 64-bit CRC of the payload ahead of the payload, require it to match before the payload is handed to Vulkan, and make every I/O failure fall back to an empty cache instead of an exception. The repository CRC family is `common::Crc` (`Common/Crc.h`), whose `crc_t` is `uint64_t` and whose array overload at `Common/Crc.h:109` hashes a pointer plus element count.

Save (`DeviceManager.cpp:337-363`): after the second successful `vkGetPipelineCacheData`, compute `common::crc_t uiCrc = common::Crc(cacheData.data(), static_cast<int64_t>(uiDataSize));` and, inside the existing `WriteFileAtomically` lambda, write `uiCrc` first and the payload second. Keep the existing atomic-write helper, the existing failure logs, and the existing `vkDestroyPipelineCache` call unchanged.

Load (`DeviceManager.cpp:382-431`): the file is an opaque external artifact, so this is a trust boundary and every rejection leaves `initialDataSize = 0` / `pInitialData = nullptr`, which is exactly the no-cache-file path the function already takes. Reject the file when any of the following holds, in this order, so no later step can consume a value an earlier step failed to produce:

- the stream is not open after `OpenFile` (`is_open()` false);
- the size query fails or is negative (`tellg()` returns `-1`, or the stream is in a failed state after the seek);
- the size is smaller than `sizeof(common::crc_t) + sizeof(VkPipelineCacheHeaderVersionOne)`;
- the size is larger than a fixed generous cap, `constexpr int64_t kiMaxPipelineCacheBytes = 64 * 1024 * 1024;` declared beside `LoadPipelineCache`, which is far above any real driver blob, so no external file can force `resize` into `std::length_error` or `std::bad_alloc`;
- the read does not deliver the full size (`gcount()` differs from the requested size, or the stream failed);
- the recomputed CRC over the bytes after the leading `crc_t` does not equal the stored one;
- the existing header compatibility check fails.

Only after the size checks pass may `cacheData.resize` be called, and its argument comes from the validated size. Read the stored CRC, then run the existing header comparison against the payload region rather than against offset zero, and point `pInitialData` at the payload region. Each rejection logs at `kDebug` through the existing discard log site, with the cause distinguished — unreadable file, CRC mismatch, or incompatible header — so the three are separable in a log. No rejection throws, and no rejection prevents boot: the renderer proceeds with a freshly created empty cache exactly as it does on a first run.

Then set `Projects/BrokenEngineSandbox/Source/Pch.h:18` to `inline constexpr bool kbVulkanPipelineCache = true;` and delete its `DT: TODO` comment.

An existing `pipeline.cache` written by the old format will fail the CRC check and be discarded once, after which the next shutdown writes the new format. That is the intended and only migration; no backward-compatibility code is added.

## Critical files

- `Engine/Source/Graphics/Managers/DeviceManager.cpp` - `LoadPipelineCache()` (currently :382) and the `~DeviceManager()` cache save (currently :337)
- `Engine/Source/File/FileManager.cpp` - `OpenFile` returning a possibly-unopened stream (currently :193)
- `Projects/BrokenEngineSandbox/Source/Pch.h` - `kbVulkanPipelineCache` (currently :18)
- `Common/Crc.h` - `common::Crc(const T*, int64_t)` (currently :109)

## In scope

- `DeviceManager::~DeviceManager`: prefixing the saved `pipeline.cache` payload with its `common::crc_t`.
- `DeviceManager::LoadPipelineCache`: the open, size-query, minimum-size, maximum-size, complete-read, CRC, and existing-header rejections above, plus the `kiMaxPipelineCacheBytes` constant they use; offsetting the header check and `pInitialData` past the prefix; ordering `resize` after the size validation.
- The discard log site in `LoadPipelineCache`, split so unreadable file, CRC mismatch, and incompatible header are distinguishable.
- `Pch.h:18`: value `true` and removal of its `DT: TODO` comment.

## Out of scope

- Any other `Pch.h` switch.
- The `VkPipelineCacheHeaderVersionOne` compatibility fields themselves and the device-property comparison they use.
- Any change to `WriteFileAtomically`, `FileManager::OpenFile`, `FileManager` path handling, or `PipelineCreator.cpp`.
- A cache-versioning, migration, or backward-compatibility path for the old format.
- Pipeline creation timing, warm-up, precompilation, or any other `mVkPipelineCache` consumer.
- Applying the same validation shape to other persisted files.

## Risk tier and invariants

Tier 3. Two triggers, each independently sufficient under the root `AGENTS.md` risk-tier rules: the change alters a persisted on-disk file layout (`pipeline.cache` gains a fixed-size CRC prefix, so old and new files are not interchangeable), which is a serialization/data-layout surface excluded from Tier 2; and it defines the validation of an opaque external file, which is a trust boundary also excluded from Tier 2.

Invariants the change exposes:

- The file is opaque external input. Every field read from it is validated before it sizes an allocation, bounds a read, or reaches a Vulkan call.
- Every failure path degrades to an empty cache. No path throws, and none can prevent the client from booting.
- Save and load formats change together in one commit; there is exactly one current format.
- The cache affects pipeline creation time only. It is client-only, outside the simulation CRC, and cannot affect determinism, the wire protocol, or save/replay.

## Acceptance criteria

- Client boots twice in a row with `kbVulkanPipelineCache = true`: the first run's save log reports a payload byte count, and the second run's load log reports a byte count exactly `sizeof(common::crc_t)` (8) larger, because the save log counts the payload while the load log counts the whole file including the CRC prefix. Neither log's contents change.
- Flipping any byte of the payload in the saved `pipeline.cache` makes the next boot log the CRC-mismatch discard and boot normally with a freshly created cache.
- Truncating the file to fewer than `sizeof(common::crc_t) + sizeof(VkPipelineCacheHeaderVersionOne)` bytes makes the next boot log the discard and boot normally.
- Truncating the file mid-payload (size above the minimum, CRC no longer matching) makes the next boot log the discard and boot normally.
- Replacing `pipeline.cache` with a file larger than `kiMaxPipelineCacheBytes` (over 64 MiB) makes the next boot log the unreadable-file discard and boot normally with a freshly created cache, with no `resize` on the oversized value.
- The incomplete-read rejection is exercised directly: with a valid `pipeline.cache` in place and a temporary local instrumentation that advances the stream by one byte before the read, so `gcount()` comes back one short of the requested size, the boot logs the unreadable-file discard and boots normally. The instrumentation is reverted before the change lands.
- The incompatible-header rejection is exercised directly: flip a byte inside the payload's `pipelineCacheUUID` field, then recompute the CRC over the modified payload and store it in the leading `crc_t` so the CRC check passes. The next boot logs the incompatible-header discard — not the CRC-mismatch discard — and boots normally.
- A `pipeline.cache` that cannot be opened or whose size query fails makes the next boot log the discard and boot normally, with no exception and no `resize` on a negative size.
- A pre-change `pipeline.cache` left in place is discarded exactly once on the first new boot and replaced on the following shutdown.
