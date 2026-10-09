<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:18:27.824Z","dependsOn":[]} -->
# Remove over-engineered checks and write protection in DataPacker

## Context

A repository-wide over-engineering sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, and ultra-rare edge-case protection (torn-write and power-loss protection, retries) that a plain operation plus an `ASSERT` or a hard error would replace. A low-effort first-pass model produced the findings; a second model validated each one once against the code (about 95% confirmed), and the dispatching session spot-checked only two of the sweep's 27 groups. For DataPacker, 54 findings survived that validation. This Plan lists 52 of them as candidates (D1-D52). The other two contradict the style guide and are under "Do not change", along with the three rejected entries.

User direction, as relayed by the dispatching session: do not trust these recommendations blindly. Every listed finding is a candidate, not an approved change. Line numbers were taken on 2026-10-08 and will drift.

## Design

Recommended procedure for the executing session:

1. For each candidate below, find the current code by symbol, not by line number. Re-derive the claim yourself by reading every caller and the invariant it relies on.
2. Apply a candidate only when you can prove it from current code: cite the invariant or every caller in the change summary. Drop any candidate that does not hold, whose proof needs more than a local reading, or whose code has changed shape. Report each dropped one with the reason.
3. The simpler forms are deletion, `ASSERT(...)`, or a hard error (`throw` or `VERIFY_SUCCESS`). `ASSERT` is always active (`Common/ErrorUtils.h:20`), so turning a check into an `ASSERT` keeps a hard stop and is an acceptable result. When a candidate's proposed form and an `ASSERT` both fit, prefer the form the candidate gives.
4. Return a per-ID table: applied (with its proof) or dropped (with its reason).

Boundaries that must never be removed, even when a candidate seems to propose it:

- Server validation of client-to-server records and of grid saves, and the bad-value rule in `.agents/references/cpp-conventions.md` (reject the input; never clamp or substitute a value).
- The per-tick determinism CRC and anything that feeds it.
- `.pack`, manifest, save, and replay format and version checks, and every exporter `kiVersion`.
- Win32 and Vulkan result checks.
- Checks on genuinely external input: user source assets (glTF, WAV, images, mesh files), shader-compiler output, Gaea output, and cache files read back from disk. A candidate that touches one of these must leave the external input still fully checked. Arithmetic made redundant by an earlier check on the same external value is fair game.

The `DataPacker/Source/AGENTS.md` "Atomic output" rule (`## Architecture`) requires jobs to write temporary files and rename them only on complete success. D6, D13, D14, D48 and D49 remove a temp-file-and-rename step from cache metadata, marker, and fingerprint files. The validator judged that the rule covers job outputs, not this cache state. That rule outranks the validator: apply these five only if you confirm the file is not an output the rule covers. Otherwise drop them and report the conflict. Do not edit the rule to make a candidate fit.

### Candidates

DiagnosticReporter.cpp

- **D1** `DataPacker/Source/DiagnosticReporter.cpp:121`: `Utf8ToWide(NormalizeUtf8(rRecord.title))` equals `Utf8ToWide(rRecord.title)`. `NormalizeUtf8` (:34-45) decodes to wide and then re-encodes to UTF-8, and every `.title` is an ASCII literal (DiagnosticReporter.cpp:136,152; ExportCubemapIbl.cpp:235; FileManager.cpp:561; Main.cpp:161,548,850,862,881,913,926). Form: `std::wstring title = Utf8ToWide(rRecord.title);`.

ExportJobs/AudioRepair.cpp

- **D2** `:78-81` `SolveNaturalCubicSpline`'s `if (iCount < 3) return;` cannot fire. The only caller, `ReconstructClipRuns` (:282), is reached only when both sides have at least `kiClipSupportMinSamplesPerSide = 4` samples (:22, :257, :275), so `iCount >= 8`. Form: `ASSERT(iCount >= 3);`.
- **D3** `:238` the `rRun.iStart == 0 || rRun.iEnd == iFrames - 1` skip repeats the support-count skip. An edge run gets zero support on that side and is skipped at :257 or :275 with the same single `iRunsSkipped` increment. Form: `if (iLength > kiClipRunMaxFixSamples)`.
- **D4** `:484-487` `if (rfSamples.empty()) return;` repeats the `iSourceFrames < 2` return (:490-493). The only caller (ExportAudio.cpp:68) passes `nChannels`, which is asserted to be 1 or 2 at ExportAudio.cpp:19. Form: delete.

ExportJobs/ExportCubemapIbl.cpp

- **D5** `:190-204` three throws guard a `lexically_relative` result that cannot be empty, absolute, or start with `..`. The path comes from `recursive_directory_iterator(cacheRoot)` (:161) and passed the regular-file filter (:165). Form: one `ASSERT(!relativePath.empty() && !relativePath.is_absolute() && relativePath.begin()->string() != "..");`.
- **D6** `:62-71` `WriteFingerprintMetadata` writes a temp file and publishes it with `MoveFileExW(... MOVEFILE_WRITE_THROUGH)`. A torn `.meta` heals itself: the `.meta.dirty` marker is removed only after the write (:117-121), and `IsOutputCurrent` returns false while it exists (:75-78). On the older path, a torn file never equals the full fingerprint (:89). Form: a direct `std::ofstream(..., binary | trunc)` write followed by `VERIFY_SUCCESS(stream.good())`. This one depends on the "Atomic output" check above.

ExportJobs/ExportIsland.cpp

- **D7** `:298-303` `CheckedProduct`/`CheckedSum` on mesh byte counts cannot overflow `int64_t`. Both counts are bounded to [1, INT32_MAX] (:279-294), so the largest value is about 3.4e10. Form: plain arithmetic with the `int64_t` casts kept, drop `iPayloadBytes`, and delete `CheckedSum` (:41) if no caller is left. Keep the exact-size check (:304).
- **D8** `:263-268` the minimum-header-size check repeats the header read failure check (:272-278). A short file still throws an error naming the file. Form: delete :265-268 and keep `kiMeshHeaderBytes` and `uiMeshFileBytes`.
- **D9** `:312,316` the `iPositionBytes > 0 &&` and `iIndexBytes > 0 &&` read guards are always true, because the vertex count is at least 1 (:283) and the index count at least 3 (:287, :291). Form: remove both prefixes.
- **D10** `:382` `CheckedProduct` on the crop pixel count cannot overflow: both dimensions are positive `int32_t` values (:368-369). Form: `int64_t iCropPixelCount = baked.iCropWidth * baked.iCropHeight;`. Keep the `> max / 4` check (:383).
- **D11** `:395-400` the elevation-dimension check cannot fail, and the elevation products cannot overflow. `kiElevationDivisor` is 4 (ExportIsland.h:8), and the crop dimensions are positive, `int32_t`, and divisible by 4 (:368-369). Form: delete :395-398, use plain products at :399-400, and delete `CheckedProduct` (:32) once it has no caller.
- **D12** `:182,193` the `iJpegQuality`/`iJpegSidecarQuality` parameters only ever receive `kiJpegSidecarQuality = 90`, a block-local constant (:448) passed at :249, :453, :463, :472, :479. Form: move the constant to file scope, drop both parameters, and drop their arguments.

ExportJobs/ExportJob.cpp

- **D13** `:40-53` the `.meta` fingerprint temp-file-and-rename protects nothing. Its only caller, `RunExport` (:296), removes `mCacheMetadataFile` first (:286). A torn file makes `ReadFingerprintMetadata` (:16-37) return `nullopt`, which leaves the job dirty. Form: open the `std::fstream` directly, delete the `MoveFileExW` (:52), and keep `VERIFY_SUCCESS(stream.good())`. This one depends on the "Atomic output" check above.
- **D14** `:117-127` the marker temp-file-and-rename protects nothing. A torn marker is a strict prefix of the fingerprint, so the comparisons at ExportIsland.cpp:583 and ExportScene.cpp:45 fail, which costs one re-run. Form: a direct `std::ofstream(..., binary | trunc)` write plus `VERIFY_SUCCESS`, deleting the :117 comment and :120-121. This one depends on the "Atomic output" check above.

ExportJobs/ExportModel.cpp

- **D15** `:101-104` the index-data-size overflow check cannot fire. `RequireSourceExtent` (:68) bounds `uiIndexBytes` by the real file size, and :79/:84 already allocated that many bytes. Form: delete.
- **D16** `:109-112` the `iIndicesSize < 0` and sum-overflow check cannot fire. `iIndicesSize` is `RoundUp<4>` of a bounded non-negative value (Common/DataFile.h:311-314), and :68-73 bound the sum by the file size. Form: delete.
- **D17** `:113-118` the chunk data-size limit check cannot fire, because `iDataSize <= iFileSize + 3`. Form: keep the `iDataSize` declaration and delete :114-118.

ExportJobs/ExportRaw.cpp

- **D18** `:30-34` the source file-size overflow throw (about 2^63 bytes) is unreachable on NTFS. Form: `ASSERT(uiFileSize <= static_cast<uintmax_t>(std::numeric_limits<int64_t>::max() - common::kiChunkDataOffset - (common::kiAlignmentBytes - 1)));`.

ExportJobs/ExportScene.cpp

- **D19** `:494-495,505-509` the local `catch (...) { CleanupTextureAttemptFiles(); throw; }` around the texture encode loop repeats the caller's cleanup. The only path is `ProcessTextures` <- `PreExport` (:418) <- `Export` (:408) <- `ExportJob::RunExport` (ExportJob.cpp:262-270), whose `catch` calls `ExportScene::CleanupOnFailure`, which calls `CleanupTextureAttemptFiles()` first (:1079). `mTextureAttemptFiles` is still populated at that point (cleared only at :528 and :1061). Form: remove the `try`/`catch` and keep the loop body.
- **D20** `:515-516,523-527` the same duplicate `catch` around the publish loop. `CleanupOnFailure` (:1077-1093) also removes the published finals through `mPublishedTextureFiles` (:520). Form: remove the `try`/`catch` and keep `mTextureAttemptFiles.clear();` (:528).
- **D21** `:644-652` the skinned-material fallback to node 0 when `iNodeIndex < 0` is unreachable. `bHasSkinning` is set only together with `iNodeIndex = iCurrentNodeIndex` (Scene/SceneVerticesLoader.cpp:94-95,106-107), and that index was already used in `nodes.at(...)` (ExportScene.cpp:603, SceneVerticesLoader.cpp:309). Form: `ASSERT(rInfo.iNodeIndex >= 0);` and then the plain assignment.
- **D22** `:656` `rInfo.iNodeIndex < std::ssize(rGltfModel.nodes)` repeats a bound already enforced by the same `nodes.at()` calls. Form: `else if (rInfo.iNodeIndex >= 0 && bHasSkeleton)`.
- **D23** `:784-789` the `iDataSize < 0 || iDataSize > iMaximumDataSize` throw cannot fire. Materials are capped at `kiMaxMaterials` = 128 (:771, Common/DataFile.h:94), and `MaterialDataOffset` (Common/DataFile.h:111-114) scales an in-memory vector size. Form: delete the local and the `if`.
- **D24** `:750` an explicit `flush()` right before `close()`. `close()` flushes, and `VERIFY_SUCCESS(good())` (:752) catches any write failure. Form: delete.

ExportJobs/ExportShader.cpp

- **D25** `:5, 91-94, 152-160, 206-214` the file-local `kbOptimizeShaders = true` toggle and its dead `-O0 -g`/`-Od -g` branches. Nothing else references the constant. Form: delete the constant and keep only the optimized branches (`spirvFile = OptimizeShader(spirvFile);`, `L" -O"`, `L" -g0"`). The SPIR-V output must stay unchanged.
- **D26** `:521-524` the in-loop "ambiguous or outside-root entry" throw in `ParseRootDelimitedDependencies` cannot fire. The single caller enters only after a root match at offset 0 (:573), and every later start is either the end of the content or `iSpace + 1`, which the break at :529-533 already root-matched. Form: `ASSERT(FindMatchingDependencyRoot(rRootPrefixes, lowerContent, iDependencyStart) != nullptr);`.
- **D27** `:650-653` the `!bFoundRoot` throw in `CaptureDependencies` cannot fire. Every path already passed `IsDependencyInInputRoot` (:537, :556; predicate at :352-363), which applies the same per-root test (:627-640). Form: `ASSERT(bFoundRoot);`.
- **D28** `:426-429` the `relativeDependencyPath.empty()` rejection in `ReadDependencyMetadata` is unreachable. `iPathCharacters > 0` (:409), and `lexically_normal` never yields an empty path. Form: delete it. Keep every other cache-file check (magic, version, count, root, length, absolute path, `..`).

ExportJobs/Island

- **D29** `BakeRoute.cpp:617` the second `remove(intermediatesDirectory / kpcSplitVersionFile)` repeats :600. Nothing in between writes that file (its only writer is :699), and DataPacker runs as a single instance (named mutex, Main.cpp:874-934). Form: delete :617 and keep the `kpcBakeVersionFile` remove (:616).
- **D30** `GaeaArchetype.cpp:17-21` the `.<pid>.tmp` suffix guards against concurrent DataPacker processes, which the named mutex already excludes. Form: `std::filesystem::path tempFile = rFile; tempFile += L".tmp";`, deleting the PID comment and keeping the atomic-replace comment and the rename.
- **D31** `SubdivideBeachBand.cpp:311-322` the `triangleAlive.at(iNeighbor*) != 0` tests are always true. The edge table holds only live triangles (`KillTriangle`, :167-179, removes the edges before it clears the alive flag), and nothing between the capture (:282-284) and the push kills a neighbor. Form: `if (iNeighborAB >= 0)` and its two siblings.

ExportJobs/Scene

- **D32** `SceneAnimationLoader.cpp:55-62` the `iComponentSize <= 0` and `iComponentCount <= 0` throws cannot fire. :30 rejects any non-FLOAT type, and both callers (:319, :327) run after the SCALAR (:286) and VEC3/VEC4 (:292) accessor checks. Form: `ASSERT(iComponentSize > 0 && iComponentCount > 0);`.
- **D33** `SceneAnimationLoader.cpp:9-16` the `uiElementSize != 0 &&` clause in `CheckedAnimationSize` guards a division that every caller makes safe (:70, :71, :310, :314, :316, :317 all pass a nonzero size). Form: `ASSERT(uiElementSize != 0);` then the plain overflow `if`.
- **D34** `SceneAnimationLoader.cpp:90-93` the final `uiRequiredBytes > size - uiBufferOffset` throw follows from :65, :72, and :76, which stay and keep the external glTF bounds fully checked. Form: delete.
- **D35** `SceneAnimationLoader.cpp:278-281` the `rInputAccessor.count == 0` throw repeats `AccessorFloats`'s "has zero elements" throw (:34), and nothing between them reads the input data. Only the error wording changes. Form: delete.
- **D36** `SceneAnimationLoader.cpp:188-191` the translation branch in `EmitKeyframes` is identical to the final `else` (:190 vs :194). Form: delete the branch.
- **D37** `SceneSkeletonLoader.cpp:166-173` the `uiDataOffset > size` and `uiAccessorSpan > size - uiDataOffset` throws follow from :143, :147, and :153-160. The overflow check at :161-164 is unreachable by the same bounds. Form: delete :166-173, and optionally :161-164.

ExportJobs/Texture

- **D38** `MigrateLegacyIntermediates.cpp:165-169` `iExpectedRawSize <= 0 || !in_range<uLong>(...)` cannot fire. Dimensions are bounded to 1..32768 and mips to 1..32 (:160), so the worst case is about 2.86e9, below the `uLong` maximum. Form: delete, or `ASSERT` the same condition.
- **D39** `MigrateLegacyIntermediates.cpp:177-180` `!in_range<uLong>(iPayloadSize)` repeats the :182 rejection: any value that large differs from `iExpectedRawSize` and exceeds `compressBound`. Form: delete.
- **D40** `MigrateLegacyIntermediates.cpp:123-127` `!in_range<int64_t>(uiFileSize)` guards a file of at least 2^63 bytes. Form: `int64_t iFileSize = static_cast<int64_t>(std::filesystem::file_size(rPath));`, optionally with an `ASSERT`.
- **D41** `Texture.cpp:107-118` the `uintmax_t` overflow throws for width*height and *2 cannot fire. The only caller (ExportIsland.cpp:451) passes positive `int32_t` crop dimensions (ExportIsland.cpp:368-372). Form: `uintmax_t uiExpectedBytes = static_cast<uintmax_t>(miWidth) * static_cast<uintmax_t>(miHeight) * sizeof(uint16_t);`. Keep the `file_size` mismatch check (:123).
- **D42** `Texture.cpp:129-132` the second byte-count check after `ReadEntireFile` only guards against the file changing between two calls. `ReadEntireFile` (Common/FileUtils.h:58-72) sizes from `file_size` and throws on a short read. Form: delete.
- **D43** `Texture.cpp:700-702` `VERIFY_SUCCESS(fileStreamOutput.good())` right after `rWriteBody` repeats the check after the flush (:704). Stream error bits are sticky. Form: delete :702.

FileManager.cpp

- **D44** `:178-185` the `AddChecked` overflow guard on summed output file sizes (its only use is :274) cannot reach 2^64 bytes. Form: delete `AddChecked` and write :274 as `inventory.uiAllocation += (uiSize + uiClusterBytes - 1) / uiClusterBytes * uiClusterBytes;`.
- **D45** `:539-541` the `eState == kLocal` early return in `MaterializeOutput` is unreachable. `EnsureLocal` (:529) returns early for `kLocal`, and `ReconcileWorktreeOutput` calls it only inside `kAbsent` (:506, :520). Form: delete, or `ASSERT(rRoot.eState != OutputRootState::kLocal);`.
- **D46** `:122` the `repositoryRoot.empty() ||` short-circuit is redundant: an empty path's `filename()` is empty and fails the same ordinal compare, which throws at :124. Form: keep only the `CompareStringOrdinal(...) != CSTR_EQUAL` test.

InputFingerprint.cpp

- **D47** `:34-37` the `mpHash != nullptr` guard in `~Sha256Hasher` is always true. The constructor (:26-29) throws when `BCryptCreateHash` fails, and the class is non-copyable (:40-41). Form: an unconditional `BCryptDestroyHash(mpHash);`.
- **D48** `:264-281` the fingerprint cache `Save` writes a PID-suffixed temp file and publishes it with `MoveFileExW(... MOVEFILE_WRITE_THROUGH)`. The single-instance mutex holds for the whole run (Main.cpp:874-935), and a torn cache fails the JSON parse in `Load()`, whose catch (:218-222) clears it and falls back to hashing file contents. Form: a direct `std::ofstream(mCacheFile, trunc)` write of `cache.dump()` that throws `std::runtime_error` naming the file on failure. This one depends on the "Atomic output" check above.
- **D49** `:383-401` the `.fingerprint.meta` write uses the same temp-file-and-write-through publish. A truncated file throws inside the :332-364 `try` and falls back to `HashFileContents`. Form: a direct `std::ofstream(metadataPath, trunc)` write that throws naming the file on failure. This one depends on the "Atomic output" check above.

Main.cpp

- **D50** `:214-217` `std::in_range<uint64_t>(uiFileSizeValue)` on a `uintmax_t` is always true on the x64-only build. Form: delete it. Keep the `in_range<std::streamoff>` check (:218-221).
- **D51** `:251-254` `std::in_range<size_t>(iManifestChunkCount)` after the non-negative check (:243) is always true on x64. Form: delete it. Keep the manifest format checks (:243-250).
- **D52** `:255-258` the `count > streamsize::max / sizeof(ChunkLocation)` guard follows from :247's bound by file size, together with :218. Form: delete it. This is a manifest reader: apply it only if every manifest format check stays.

### Do not change (validated as REJECTED or contradicting the style guide)

- `DiagnosticReporter.cpp:61` `.at()` inside a bounded loop: `Documents/C++StyleGuide.txt` rule 16 (`:130`) requires `std::vector` access through `.at()`.
- `ExportModel.cpp:91` `indices16.at(i)` / `indices32.at(i)`: same rule 16.
- `AudioRepair.cpp:478` / `AudioRepair.h:26` `iTargetRate`: an ordinary resampler input, not a defensive check.
- `MigrateLegacyIntermediates.cpp:102-105` (`rgba8.at(...)` to `operator[]`) and `InputFingerprint.cpp:101-123` (CRLF loop `.at()` to `operator[]`): the validator confirmed both, but they contradict rule 16, which the validator itself cited to reject the two `.at()` entries above. The style guide outranks the validator, so both stay as `.at()`.

## Critical files

- `DataPacker/Source/DiagnosticReporter.cpp`, `DataPacker/Source/FileManager.cpp`, `DataPacker/Source/InputFingerprint.cpp`, `DataPacker/Source/Main.cpp`
- `DataPacker/Source/ExportJobs/AudioRepair.cpp`, `ExportCubemapIbl.cpp`, `ExportIsland.cpp`, `ExportJob.cpp`, `ExportModel.cpp`, `ExportRaw.cpp`, `ExportScene.cpp`, `ExportShader.cpp`
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp`, `GaeaArchetype.cpp`, `SubdivideBeachBand.cpp`
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp`, `SceneSkeletonLoader.cpp`
- `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp`, `Texture.cpp`
- `DataPacker/Source/AGENTS.md` (read for the "Atomic output" rule; edit only if a sentence becomes false)

## In scope

- Only the code regions named by candidates D1-D52, each changed only to the stated simpler form or an equivalent `ASSERT`/hard error, and only after the executor's own proof.
- Removing a helper (`CheckedSum`, `CheckedProduct`, `AddChecked`, `kbOptimizeShaders`, a parameter) that a candidate leaves with no caller.
- A comment adjacent to a changed region that the change makes false.

## Out of scope

- Every boundary in `## Design`, the "Do not change" list, and any candidate the executor cannot prove.
- Any change to `.pack`, manifest, generated-header, or cache-file byte formats, or to an exporter `kiVersion`. If a candidate turns out to need one, drop it.
- Any change to the `DataPacker/Source/AGENTS.md` "Atomic output" rule.
- New findings beyond D1-D52. Report them instead of applying them.
- `Engine/`, `Common/`, and `Projects/`, including `Common/` helpers that a candidate cites as evidence.

## Acceptance criteria

- DataPacker Release builds through `/compile` with no new warnings.
- The change summary lists every ID D1-D52 as applied (with the cited invariant or callers) or dropped (with the reason).
- No exporter `kiVersion` changes, and no boundary in `## Design` is weakened.
- Running DataPacker after the change completes the export without new errors. The DataPacker change makes Local data mode mandatory for any game build (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`). The executor should request Local generation authorization at plan approval if it wants to run the export through a game build.

## Notes

- Change Workflow tier: Tier 2. Trigger: scoped behavior inside one subsystem (DataPacker error paths, cache-metadata write mechanics, and one speculative toggle), with no change to determinism, wire, pack or save format, threading, or trust boundaries. A reviewer should escalate to Tier 3 if any applied candidate changes the bytes of an exported pack, manifest, or generated header.
- D30 (GaeaArchetype) keeps its atomic rename and drops only the PID suffix, so the "Atomic output" rule does not affect it.
