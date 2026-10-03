<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:26:47.927Z","dependsOn":[]} -->
# Use std::array for priority texture CRCs

## Context

`TextureManager::PriorityTextures` in `Engine/Source/Graphics/Managers/TextureManager.h` is a one-field wrapper around `common::crc_t pCrcs[kiPriorityTextureCount]`. It exists to return the built-in array from a consteval lambda. The wrapper requires three builder projections and projections at both consumers despite both APIs accepting `std::span<const common::crc_t>`.

The table contains 27 CRCs: two entries from `kpPriorityHead`, 17 from `kpWaterNormalCrcs` (`shaders::kiWaterNormalCount` in `Engine/Data/Shaders/ShaderLayoutsBase.h`), and eight from `kpPriorityTail`. `common::crc_t` is `uint64_t` in `Common/Crc.h`. Using `std::array` provides the required value semantics directly and removes the bespoke wrapper without changing boot loading.

## Design

Delete the nested `PriorityTextures` declaration. Declare the existing constant as `static inline constexpr std::array<common::crc_t, kiPriorityTextureCount> kpPriorityTextures = []() consteval` and retain its immediately invoked lambda initializer. Declare its local as `std::array<common::crc_t, kiPriorityTextureCount> priorityTextures {};`. Replace each `priorityTextures.pCrcs[i++] = crc` with `priorityTextures[i++] = crc`.

Keep the count expression, local `int64_t i = 0`, three range-for loops, CRC element type, head/water/tail order, and returned local unchanged. Keep all source table names and the `kpPriorityTextures` name. `<array>` is already supplied by `Common/ExternalHeaders.h`; add no include or alias.

Pass `kpPriorityTextures` directly to `gpFileManager->RequestChunkLoad` in `TextureManager::InitializeBootTextures`, preserving `LoadPriority::kRealtime`. Pass `TextureManager::kpPriorityTextures` directly to `gpTextureManager->WaitForTextures` in `MainThread`. The existing dynamic-extent, const-element span parameters accept the const standard array; no explicit span construction, pointer/count pair, overload, or API signature change is needed.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.h`: wrapper, constexpr table builder, and `WaitForTextures(std::span<const common::crc_t>)` declaration.
- `Engine/Source/Graphics/Managers/TextureManager.cpp`: `TextureManager::InitializeBootTextures` priority request.
- `Engine/Source/Main.cpp`: `MainThread` priority wait.
- Read-only contracts: `Engine/Source/File/FileManager.h` (`RequestChunkLoad` span declaration), `Common/Crc.h`, `Common/ExternalHeaders.h`, `Engine/Data/Shaders/ShaderLayoutsBase.h`, `Documents/C++StyleGuide.txt` rule 21, and `Engine/Source/Graphics/Managers/AGENTS.md` TextureManager contract.

## In scope

- Remove `TextureManager::PriorityTextures` and replace only its `kpPriorityTextures` constant and consteval local with the explicit standard array type.
- Remove the five `.pCrcs` projections belonging to this table: three writes and two span arguments.
- Verify unchanged compile-time contents, static lifetime, and span pointer/count behavior.

## Out of scope

- Modernizing any other built-in arrays, including the head, water, tail, and IBL tables; renaming constants; extracting builders or aliases.
- Changing counts, CRC values, resource priorities, upload sequencing, wait placement, timing scopes, FileManager or TextureManager function signatures, or project membership.
- Changes to simulation, shaders, serialization, pack formats, network formats, asset generation, or persistent data.
- Unit tests, runtime instrumentation, compatibility paths, and unrelated cleanup.

## Risk and invariants

Tier 2: the public constant's C++ type changes and its uses cross the texture manager's boot request and MainThread startup wait, although this is behavior-preserving and the two consuming function signatures remain unchanged. The representation is a client-only static constant, not a TextureManager instance member or persisted/CRC-bearing frame field; no instance layout, determinism, serialization, threading, or trust-boundary contract changes.

Preserve all 27 values in exactly the current order, with static storage lifetime. Do not duplicate the water-normal list. Preserve the priority request after IBL boot setup and the startup wait within its current BootStart/BootStop scope before boot interpolation/rendering proceeds. Both consumers must continue receiving a span of 27 contiguous `common::crc_t` elements.

## Performance

The consteval lambda forces construction at compile time. The standard array owns fixed inline storage and introduces no allocation, dynamic initializer, runtime concatenation, or new per-frame work. Its payload remains 27 uint64_t values (216 bytes on the supported target), and each consumer continues to receive only the data pointer and count. Verification checks these already-selected invariants; no runtime benchmark or measurement-dependent design choice is required.

## Documentation and style

No documentation or style-policy edit is needed. Rule 21 already permits `std::array` where value semantics help; this removal is precisely that case. The existing TextureManager boot-loading contract and Engine/Source startup ordering contract remain accurate. Keep the existing priority-table comment because its preloading purpose and single-source water-block rationale still apply. Apply the normal C++ style/comment review and AGENTS synchronization checks only to this narrow change; do not add a new rule or repeat the implementation in AGENTS.md.

## Verification

1. Compare the changed table builder and source lists with the implementation session baseline. Confirm the source CRC lists and count expression are unchanged, the count is 2 + 17 + 8 = 27, and each loop still appends one CRC in order. Search the repository for `PriorityTextures`, `kpPriorityTextures`, and table-related `.pCrcs` uses; the wrapper and its projections must be gone with both intended consumers updated.
2. Through `/compile`, build BrokenEngineSandbox client Debug and Release. These builds must accept both implicit conversions to `std::span<const common::crc_t>` and the immediately invoked consteval array builder. The whole table remains inside `BT_CLIENT`; a server runtime run is unnecessary.
3. Inspect the supported build's emitted table and the two call sites, comparing baseline artifacts where needed. Confirm the same 27 CRC values/216-byte payload, pointer and count 27 passed to both existing APIs, and no table-related dynamic initialization or allocation. Compare relevant payload and operations rather than whole executable bytes or relocated addresses. Record the inspected configuration and evidence locations.
4. Perform normal changed-range correctness, affected-caller, style, comment, and documentation checks. No unit tests or harness scenario is needed: this change is settled by the preserved inputs/consteval algorithm, client compilation, and focused emitted-data/call-site evidence.

## Acceptance criteria

- `PriorityTextures` is removed; the constant and builder local use `std::array<common::crc_t, kiPriorityTextureCount>` without a replacement wrapper or alias.
- The existing consteval builder yields the unchanged ordered 27-CRC table and both consumers accept it directly as the existing const span type.
- Realtime load priority, startup wait location, boot profiling scopes, static storage, and client-only affinity are unchanged.
- Client Debug and Release builds pass; focused output inspection confirms unchanged payload and pointer/count behavior with no runtime table construction or allocation.
- Only the three specified source regions change; the existing documentation and style policies remain accurate without edits, and no unit tests or runtime instrumentation are added.
