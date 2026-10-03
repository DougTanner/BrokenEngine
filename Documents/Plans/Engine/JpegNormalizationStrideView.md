<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:26:09.173Z","dependsOn":[]} -->
# Express JPEG red-channel normalization with views::stride

## Context

C++23 inventory F089 (`std::views::stride`) has one concrete clarity application in `Texture::SaveJpegSidecar`, `DataPacker/Source/ExportJobs/Texture/Texture.cpp:575`. The auto-normalization scan at lines 589-595 maintains `pfScan` and an otherwise unused loop counter solely to read every fourth float. Replacing this bookkeeping with a typed range-for states the red-channel selection directly. This is a local readability improvement, not a defect fix or a measured performance improvement.

The current storage contract is sufficient: loaders and the byte-buffer constructor allocate four floats per pixel (`Texture.cpp:58,82,137,235,258`); `Crop` replaces mip zero with exactly four floats per cropped pixel and updates dimensions (lines 309-325); `MakeMipmaps` appends separate vectors (line 300); `Downsize` erases earlier levels and shifts dimensions together (lines 355-369). External `mData` uses in DataPacker read existing storage rather than extend mip zero. The sole sidecar caller is `MaskMipSaveTexture` in `DataPacker/Source/ExportJobs/ExportIsland.cpp:183-190`, after masking and mip generation. Thus the existing dimension-counted scan and a stride-four view of `mData.at(0)` visit the same sequence.

## Design

The author's recommendation is to replace only the `bAutoNormalize` scan with:

```cpp
for (float fRed : rPixels | std::views::stride(4))
{
	fMin = std::min(fMin, fRed);
	fMax = std::max(fMax, fRed);
}
```

Remove `pfScan`, its increment, and the scan's counter. Keep `iPixelCount` because the later RGB conversion loop still uses it. Keep the infinities initializing `fMin`/`fMax`, the order and argument order of both reductions, the constant-image fallback, and every subsequent statement unchanged. In particular, do not substitute a `minmax` reduction: sequential NaN and equal-value handling must remain the same.

Add `<ranges>` once to `Common/ExternalHeaders.h` between `<random>` and `<ratio>` if absent at implementation time. The view borrows the existing vector for this loop; the vector is neither mutated nor outlived. Literal four satisfies the positive-stride precondition. No pixel buffer, callback, helper abstraction, or additional traversal is needed.

Existing `Documents/C++StyleGuide.txt` rules 8 (prefer range-for) and 15 (explicit range-for element type) already govern this adoption. `Common/AGENTS.md` `## Headers, Validation, and Platform` already owns header centralization. The texture documentation's contracts remain accurate; recommend no style-guide or AGENTS.md edits and no version bump.

## Critical files

- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — `Texture::SaveJpegSidecar` normalization scan; constructors, crop and mip operations provide the storage proof.
- `Common/ExternalHeaders.h` — centralized standard-library include list.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — read-only caller evidence in `MaskMipSaveTexture`.
- `Documents/C++StyleGuide.txt`, `Common/AGENTS.md`, and `DataPacker/Source/ExportJobs/Texture/AGENTS.md` — existing policy and contract owners, read-only.

## In scope

- The `bAutoNormalize` red-channel scan in `Texture::SaveJpegSidecar` only.
- A single `<ranges>` include in the existing standard-header list if still needed.

## Out of scope

- RGB output conversion, image loading, dimensions, masking, crop or mip behavior, JPEG options and encoder calls.
- Triangle loops in `CropAndRepackMesh` or `BeachSubdivider`, other stride candidates, generalized sampling helpers, assertions, validation or storage repairs.
- Runtime engine/game code, serialization, export versions, `.pack` formats, public signatures and documentation expansion.
- Unit tests, full asset export, Gaea baking, client/server launch and performance claims.

## Risk and invariants

Future change classification: Tier 1, a mechanical local behavior-preserving traversal replacement with no public signature or invariant change. Centralizing a standard header does not introduce cross-subsystem behavior. The base-level red sequence remains indices `0, 4, ..., 4 * (miWidth * miHeight - 1)` in the same order. Floating-point operations, operand order, equal-value/NaN handling, and constant-image fallback remain identical. RGB/JPEG bytes stay unchanged for valid input and fixed options. The traversal introduces no allocation or ownership change. Simulation determinism/CRC, replay, threading and on-disk contracts are unaffected.

## Acceptance criteria

1. The normalization scan uses exactly one typed `float fRed` range-for over `rPixels | std::views::stride(4)` and has no manual scan counter or pointer.
2. The old and new scan consume exactly the same red values in the same order; initialization, reduction operand order and fallback expression are unchanged.
3. The output conversion loop, pixel count, encoder invocation and all other function behavior are unchanged; the only other source edit is the centralized include if needed.
4. DataPacker compiles successfully with the repository C++23 toolchain.

## Verification

- Review the implementation diff and the storage-producing functions against criteria 1-3. Confirm no supported producer stores extra floats in mip zero; this plan relies on the existing exact-RGBA representation, not a new validation policy.
- Record a focused old/new scan trace, without adding tests: use interleaved RGBA examples whose red planes contain negative and positive values, a constant value, mixed finite/NaN values, all NaNs, and signed zeros. Give non-red channels distracting extreme values. Compare visited indices and the successive min/max results, including first-operand retention for NaNs and equal zeros. This settles the reduction equivalence; do not send NaNs through the unrelated float-to-byte conversion.
- Build DataPacker Release|x64 through `/compile`. No asset export or runtime harness work is needed for this offline, equivalent traversal.
- Apply the normal C++ review, style/comment review, affected-code and documentation checks required by the Change Workflow. Finalize through `/finalize-changes` only when executing and landing the future change.

## Notes

No prerequisite plans or mandatory coordination constraints are needed. Other independently landable range adoptions can share the same idempotent `<ranges>` insertion; implementations retain one include if it is already present.

Language evidence: [WG21 P1899R3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1899r3.html), sections 5.3 and 6.2, describes borrowing, the positive stride requirement and iterator advancement. [Microsoft C++ conformance table](https://learn.microsoft.com/en-us/cpp/visual-cpp-language-conformance) records P1899R3 support since VS 2022 17.4, preceding the repository's VS 2026 C++23 target. The compile check verifies the actual configured toolchain. No benchmark is needed to justify removing local pointer/counter bookkeeping.
