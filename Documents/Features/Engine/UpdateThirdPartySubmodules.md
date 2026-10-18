# Update ThirdParty Submodules

Revisit When: the user opens a window in which every other agent session is closed or idle.

## Context

Moves every ThirdParty submodule to the newest commit of its newest mainline
branch, adds `branch = <name>` to every `.gitmodules` entry, adds nlohmann/json
as its own submodule, ports our code, verifies, and commits on `main`. Today 21
submodules live under `ThirdParty/` (`.gitmodules:1-64`) and only lz4 carries a
branch line (`.gitmodules:49`, `release`). nlohmann::json 3.10.4 comes from the
copy tinygltf v2 vendors (`Common/ExternalHeaders.h:317-323`,
`Tools/ToolCommon/ToolCliCommon.h:19-24`).

The work runs in the primary checkout `C:\Users\dougt\Documents\BrokenEngine` on
`main`, never in a session worktree. A linked worktree consumes primary's
submodule trees and ThirdParty Output through link farms, so it cannot build a
changed pin: provisioning rejects any pin that differs from primary
(`.agents/scripts/Provision-WorktreeThirdParty.ps1:227-231`), and
`-Target ThirdParty` builds only in primary
(`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:252-258`). Provisioning
returns early in primary (`Provision-WorktreeThirdParty.ps1:157-160`), so
`/compile` there builds every target against primary's own trees.

Upstream facts below come from `git ls-remote`, `gh api`, and blobless clones under `Temp/upstream/<lib>.git` in session worktree `d09942d1-84a2-4805-b13c-a36e4f232b3c`, gathered 2026-10-15. Those clones are machine-local and ignored; the executing session re-reads upstream. At authoring time primary `main` was `f1d140ae` and its 21 gitlinks equal those at `e202a3dc`.

## Decisions

The authoring agent records these as the user's choices from the 2026-10-15 session (`Documents/Plans/AGENTS.md` `## Authorship`):

- Run location: the primary checkout, with every other session closed or idle for the whole procedure, because the link farms point straight into primary's submodule trees and Output (`Provision-WorktreeThirdParty.ps1:261-273`), so moving them changes every live worktree's sources and libraries. Afterwards other worktrees rebase onto the new primary tip, the documented pin-mismatch remedy (`Provision-WorktreeThirdParty.ps1:231`, `Documents/FreshMachineSetup.md:31`).
- Branch rule: per library, the head of the mainline branch (default, main/master, dev/develop, or a release line) with the newest head. Topic, bot, per-feature, and gh-pages branches never qualify. The table in `## Branch choices` stands. The executing session re-verifies heads, records each SHA when it sets that gitlink, and checks the commit against that record, not against a later `ls-remote`.
- tinygltf: the gitlink moves to the `release` head, but we keep consuming the
  v2 C++ API from the head's `attic/tiny_gltf.h`. Upstream `5aa461e` ("Deprecate
  legacy v1/v2/v3 C++ API; make v3 C the mainline") moved it there. Its diff
  against our pinned `tiny_gltf.h` is 28 lines, all in the implementation's
  JSON-backend selection plus one `urldecode` cleanup, none in the public API.
  The vendored `stb_image.h`, `stb_image_write.h`, and `json.hpp` only moved.
  Every tiny_gltf include site defines `TINYGLTF_NO_INCLUDE_JSON` and includes
  nlohmann `single_include/nlohmann/json.hpp` first, so only one nlohmann
  version is compiled. No DataPacker glTF code changes and no v3 C port.
  Trade-off accepted: the attic copy is deprecated and untested upstream, and
  later tinygltf updates may stop carrying it. The first DataPacker build
  settles whether `attic/tiny_gltf.h` compiles against nlohmann develop.
- nlohmann/json: new submodule `ThirdParty/json` (`https://github.com/nlohmann/json.git`, branch `develop`), consumed only through `single_include/nlohmann/json.hpp`. The dependency is approved (`ThirdParty/AGENTS.md` `## License Policy` step 4), with the license review in `## License review`. The C5311 pragmas go away (`## API-break inventory`).
- openexr `main`: the unity wrapper and config shims are reworked, the bundled libdeflate (MIT) is compiled, and OpenJPH and zstd are not. Link stubs cover all four entry points `compression.c` references.
- mimalloc `dev3`: the startup-reserve check at `Engine/Source/Memory/GlobalAllocator.cpp:250-257` checks `mi_stats_get` reserved and committed against `kiMimallocArenaReserveMebibytes`, and keeps the warning path.
- imgui `master`: `Engine/Source/Ui/MenuUtils.cpp:310` gets the new `AddRect` argument order, and `Engine/Source/Graphics/Managers/DeviceManager.cpp:270-271` reserves ImGui's two samplers. If implot `master` does not build against imgui `master` (WIP), the executing session stops and asks the user.
- Upstream symbolic links: the openexr `main` head tracks
  `src/test/OpenEXRFuzzTest/oss-fuzz -> ../oss-fuzz` and the tinygltf `release`
  head tracks `attic/examples/saver/Makefile -> Makefile.dev` (mode `120000`);
  neither current pin has one. Primary runs with `core.symlinks=true`, so
  checking these heads out would create reparse points inside the submodule
  trees, and every linked worktree's provisioning would then throw
  `contains unsupported reparse point`
  (`Provision-WorktreeThirdParty.ps1:62-77`). So `core.symlinks` is set to
  `false` in the two submodules' own config before their checkout (step 6), and
  Git writes each link as a plain file holding the link text. The recorded mode
  stays `120000` and status stays clean. `Documents/FreshMachineSetup.md` gains
  the same settings for a fresh clone. Trade-off accepted: the setting is
  machine-local, and a future upstream link in any other library fails
  provisioning the same way.
- No attribution change (`DataPacker/Source/Attribution.cpp`).
- Verification: a full forced DataPacker re-export under temporary bumps of
  every output-affecting exporter version (`## Execution procedure` step 12),
  comparing every output file the manifests describe, never the tracked
  `.PreExport` markers (they store the version, e.g.
  `Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf.PreExport` ends
  in `"version":992}`). After that come the full build matrix, an
  `/agent-harness` smoke run, and a replay determinism check. Each submodule's
  `git -C ThirdParty/<lib> status --porcelain` must be empty before the commit.

## Branch choices

Heads as seen 2026-10-15. Committer dates carry the upstream offset. `URL` is unchanged for every existing entry, and Clipper2 keeps its SSH URL.

| Submodule | Branch | Head SHA seen | Head date | Current pin | Other mainline candidates (why not) |
|---|---|---|---|---|---|
| Clipper2 | main | f9c5eb6e | 2026-04-20 | f9c5eb6e | only branch; at head |
| DirectXTK | main | 524c655e | 2026-10-06T11:20-07:00 | dc30f6b1 | others are topic |
| PerlinNoise | master | bdf39fe9 | 2021-12-04 | bdf39fe9 | v3 older; at head |
| SPIRV-Cross | main | 300681bb | 2026-10-14T13:31+02:00 | 97709575 | vulkan-sdk-1.4.363 f11ba9f0 (2026-09-16) older |
| StackWalker | master | 7af40240 | 2025-08-04 | 7af40240 | only branch; at head |
| bc7enc_rdo | master | b9438627 | 2026-07-30T22:35-04:00 | e6990bc1 | only branch |
| cmft | master | cad5f31b | 2019-08-05 | cad5f31b | others topic; at head |
| enet | master | 5a9c537f | 2026-06-23T15:17-04:00 | 8be2368a | only branch |
| gli | master | 3542f883 | 2026-04-05T11:51+02:00 | 779b99ac | version/topic only |
| glm | master | 6f14f479 | 2026-04-07T13:38+02:00 | 8d1fd52e (`1.0` line) | 1.0 f451d0ad (2026-01-12) older |
| imgui | master | 9d9a621f | 2026-10-15T19:42+02:00 | 21d3299e | docking is a feature branch |
| implot | master | 851cdbdd | 2026-10-10T17:13+02:00 | bd99f8d8 | feat/fix/features topic |
| lz4 | dev | 48bfbfe3 | 2026-10-14T21:46-07:00 | ebb370ca (`release`) | release (2024-07-21) older; dev is default |
| meshoptimizer | master | f0a684f1 | 2026-10-14T10:27-07:00 | f6ab3ec0 | topic, gh-pages |
| mimalloc | dev3 | 6f45cc52 | 2026-10-15T13:35-07:00 | 8ff03b63 (v2 `main`) | main3 (default), dev, dev2, main older; dev3-* per-feature |
| openexr | main | 471f3c0b | 2026-10-15T17:39-04:00 | 420da6a5 (RB-3.1) | RB-3.5 69b2604f (2026-10-02) older |
| scb-check | main | a8618228 | 2026-05-26T15:41-05:00 | f808b0fb | bot/topic |
| stb | master | 2c980bb5 | 2026-08-01T23:42-07:00 | f1c79c02 | dev (2023) older |
| tinygltf | release | 0d9ce1a9 | 2026-07-31T22:17+09:00 | 81bd50c1 (v2.9.x) | v3_c, v3 older; default `add-cmake-test-target` is topic |
| tinyobjloader | release | 45636bdc | 2026-06-19T19:42+09:00 | a4e519b0 | devel, v3, v1.0.x older |
| zlib | develop | 767c4c94 | 2026-09-20T20:39-07:00 | 51b7f2ab | master da607da7 (2026-02-17) older; develop is default |
| json (new) | develop | 5683f5e0 | 2026-10-15T22:45+02:00 | none | master 55f93686 (2025-04-11) older; develop is default; topic branches |

## Execution procedure

The executing session classifies the change Tier 3 and follows `.agents/references/change-workflow.md`, with this document as the plan the user approves at Step 1 and the roles in `## Review roles`. `<primary>` below is `C:\Users\dougt\Documents\BrokenEngine`, and every script runs from it in the canonical form (root `AGENTS.md` `### Directives`).

1. Window. The user closes or idles every other Claude Code, Codex, and OpenCode session and every running BrokenEngineSandbox client or server, and starts none until step 19. Run `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Wait-AgentToolsQuiescence.ps1 -RepositoryRoot '<primary>'`, and continue only on disposition `quiescent`.
2. Session. The user starts the agent CLI directly at `<primary>`, not through the session wrapper, which would create a worktree (`Documents/FreshMachineSetup.md:45`). Confirm `git status --porcelain` is empty on branch `main`, and record `HEAD` as `<start>`. `<start>` is the explicit `-Baseline` for every `/compile` brief, for `Invoke-StaticChecks.ps1`, and for review scopes.
3. Baseline data. Run
   `pwsh -NoProfile -File .agents/scripts/Bootstrap-AgentTools.ps1 -RepositoryRoot '<primary>'`,
   the primary maintenance this plan authorizes. On the unchanged tree it
   rebuilds the AgentTools, ThirdParty Debug/Profile/Release, and the DataPacker
   prebuild, and refreshes primary data (`Bootstrap-AgentTools.ps1:124-253`). It
   skips every build on a dirty primary (`:49-57`), so it runs only on a clean
   tree. Then copy the 23 files of
   `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data`
   (`.agents/skills/compile/references/runtime-data-mode.md`
   `## Generated Data directory inventory`) to
   `Temp/ThirdPartyUpdate/BaselineData/`.
4. Branch lines. For each of the 21 existing entries, run `git config -f .gitmodules submodule.ThirdParty/<lib>.branch <branch>` from `## Branch choices`; lz4 changes from `release` to `dev`.
5. Re-verify. For each of the 22 repositories, list
   `git ls-remote --heads <url>` and confirm the chosen branch is still the
   newest mainline head. If a head moved or a newer mainline branch appeared,
   take it, update `.gitmodules`, and re-run that library's
   `## API-break inventory` diff before porting. For each re-verified head, run
   `git ls-tree -r <sha>` in the submodule repository when the head is local
   there, and otherwise in a blobless clone under `Temp/upstream/<lib>.git`,
   which json always needs because it has no repository until step 6: if a
   mode-`120000` entry appears in any library other than openexr and tinygltf,
   stop and ask the user.
6. Move pins. First run `git -C ThirdParty/openexr config core.symlinks false`
   and `git -C ThirdParty/tinygltf config core.symlinks false`, which write each
   submodule's own machine-local config (`## Decisions`). Then run
   `git submodule update --remote --checkout -- ThirdParty/<lib>` for the 21
   entries, then
   `git submodule add -b develop https://github.com/nlohmann/json.git ThirdParty/json`.
   Immediately record each `git -C ThirdParty/<lib> rev-parse HEAD` with its
   committer date in `Temp/ThirdPartyUpdate/Pins.txt`, then
   `git add .gitmodules ThirdParty/<lib>` for all 22. Confirm
   `Get-ChildItem -LiteralPath <path> -Force -Recurse -Attributes ReparsePoint`
   prints nothing for every one of the 22 submodule trees, the paths
   `git config -f .gitmodules --get-regexp '\.path$'` lists, including
   `ThirdParty/json`. `--remote` re-resolves each branch head, so a head that
   moved after step 5 can bring a link step 5 never checked; any output from
   this scan means stop and ask the user. This mirrors the check provisioning
   runs on each submodule (`Provision-WorktreeThirdParty.ps1:62-77`), which
   itself returns early in primary.
7. Port. Change our code and `Documents/FreshMachineSetup.md` as `## In scope` lists, using `## API-break inventory`.
8. Build ThirdParty, then AgentTools. Through `/compile`, build ThirdParty Debug, Profile, and Release in place in primary; ThirdParty builds are allowed there. Then build WorktreeCli and AgentHarness, which are Release-only candidates under `Temp`. Compile errors in our wrappers or consumers are ported in our code only. If implot fails against imgui `master`, stop and ask the user.
9. Build DataPacker and the game. Build DataPacker Release; the build script refuses any other DataPacker configuration (`.agents/skills/compile/SKILL.md` `## Inputs`). Then build Client and Server in Debug, Profile, and Release, in the data mode `Resolve-CompileContext.ps1` reports.
   In primary, Local and Shared both use primary's `Output/Data`; a Local build there also re-exports into it under the Gaea guard and deletes the primary data stamp, which step 19's refresh writes again (`runtime-data-mode.md` `## Mode selection`).
10. Propagate and check. Run `/update-affected-code`, then `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<primary>' -Baseline <start>`.
11. Review. Run the `## Review roles` passes, resolve accepted findings, and rebuild what they touch.
12. Forced re-export in Local mode (the version bumps below make `Resolve-CompileContext.ps1` report Local), with Gaea forbidden (no `-AllowGaeaExport` and no `-ForbidExpensiveExport`).
    - Add `1'000'000` to the base literal of each of these (e.g. `Version(31)`
      becomes `Version(1'000'031)`): `ExportAudio::kiVersion`
      (`DataPacker/Source/ExportJobs/ExportAudio.h:13`),
      `ExportIsland::kiVersion` and `kiTextureVersion` (`ExportIsland.h:26,39`),
      `ExportModel::kiVersion` (`ExportModel.h:14`), `ExportRaw::kiVersion`
      (`ExportRaw.h:13`), `ExportScene::kiVersion` (`ExportScene.h:17`),
      `ExportShader::kiVersion` (`ExportShader.h:13`),
      `ExportTexture::kiVersion` (`ExportTexture.h:15`), `kiSplitVersion`
      (`Island/BakeRoute.cpp:38`), and `kiCubemapIblFingerprintVersion`
      (`ExportCubemapIbl.cpp:8`).
    - Leave alone `kiBakeVersion` (`BakeRoute.cpp:37`, Gaea) and the cache-bookkeeping versions (`ExportJob.cpp:9`, `ExportShader.cpp:319`, `InputFingerprint.cpp:86,90`).
    - Run `/compile -Target Client -Configuration Release`, whose envelope reports `/p:DataBuildMode=Local` and `/p:RunDataPacker=true`. If the Gaea guard blocks a route, stop and report it.
    - Byte-compare all 23 `Output/Data` files with `Temp/ThirdPartyUpdate/BaselineData/`, and confirm `git status` shows no tracked change under `Engine/Data` or `Projects/BrokenEngineSandbox/Data` other than the four `.PreExport` markers.
    - Report any difference to the user before the commit, with the owning version bump (`DataPacker/Source/ExportJobs/AGENTS.md` `## Shared Pipeline`, `Island/AGENTS.md` `## Versioning`). Nothing differing lands without the user's approval.
13. Revert the re-export.
    - Revert every bump and `git restore` the four `.PreExport` markers.
    - Delete the cache entries that carry the temporary versions under `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<project>`: `*.v<temp>.chunk`, `*.v<temp>.meta`, and `*.v<temp>.deps.meta` (`ExportJob.cpp:148,153`, `ExportShader.cpp:167`), plus `*.t<temp>.textures` (`ExportIsland.cpp:510`).
    - Re-run the step-12 generation build at the real versions only while `Resolve-CompileContext.ps1` reports Local. It re-splits and re-convolves once, because `SplitVersion.meta` and the IBL `.meta` files are not named by version, and its outputs must still equal the snapshot.
14. Harness. Through `/agent-harness`, run a smoke run and a replay determinism check against primary's executables.
15. Pre-commit checks. Confirm the following, then fill the acceptance table from `## Acceptance criteria`:
    - `git -C ThirdParty/<lib> status --porcelain` is empty for all 22 submodules.
    - The staged gitlinks in `git ls-files -s ThirdParty` equal `Temp/ThirdPartyUpdate/Pins.txt`.
    - `git diff --cached` holds only `## In scope` paths, with no temporary bump and no `.PreExport` change.
16. Confirmation. Present the change summary, the acceptance table, and its non-PASS rows, then ask the user to confirm committing on `main` in primary. Only an explicit affirmative reply authorizes the commit.
17. Commit. Run `git commit` on `main`, with a message ending in the executing session's attribution trailer. No landing lock or metrics-history row is written: a commit that reaches `main` without `/finalize-changes` gets no row of its own unless it becomes a later run's `BaseCommit`, as a squash result does, and the next row's diff still counts its C++ change (`.agents/skills/code-quality-metrics/references/HistoryContract.md` `## Source table and decision`).
18. AgentTools promotion (`.agents/skills/finalize-changes/references/agenttools.md`). `Tools/ToolCommon/ToolCliCommon.h` changed, so promotion is required.
    - Re-run `Wait-AgentToolsQuiescence.ps1` as in step 1.
    - Build WorktreeCli and AgentHarness Release candidates from the commit through `/compile`.
    - Run `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-AgentToolsPromotion.ps1 -PrimaryRoot '<primary>' -WorktreeCliCandidate '<candidate>' -AgentHarnessCandidate '<candidate>' -LandedCommit '<commit>'`.
19. Primary refresh. Run `Bootstrap-AgentTools.ps1 -RepositoryRoot '<primary>'` once more, on the now-clean primary. It rebuilds ThirdParty Debug/Profile/Release, the DataPacker prebuild and its stamp, and the Shared data with its data stamp, all from the commit. Then confirm `git status --porcelain` is empty, and tell the user that other sessions may resume: each worktree rebases onto the new primary tip and ports its own code if needed, for example imgui `AddRect` order or the json include.

## API-break inventory

Signatures were compared between the old-pin and new-head public headers in the upstream clones. Upstream paths below are inside the named library. An identifier-removal scan of every consumed public header against our tree found nothing else we use.

- tinygltf: `Common/ExternalHeaders.h:379-381` and `ThirdParty/Prebuilts/Source/DataPacker/tinygltf.cpp:7` include `tinygltf/attic/tiny_gltf.h` after `#define TINYGLTF_NO_INCLUDE_JSON` and the json include. The `attic/tiny_gltf.h:1713-1719` guard skips its own `json.hpp`, and `:1874` uses `nlohmann::json`. The comment at `ExternalHeaders.h:379-380` is corrected. The DataPacker glTF consumers stay as they are.
- nlohmann::json 3.10.4 to develop `5683f5e0`. Its version macros still read 3.12.0 (`json.hpp:84-86`).
  - Includes: `Common/ExternalHeaders.h:317-323` (BT_ENGINE and BT_DATA_PACKER) and `Tools/ToolCommon/ToolCliCommon.h:19-24` switch to `json/single_include/nlohmann/json.hpp`, and their comments name the `ThirdParty/json` submodule. Every project already has `ThirdParty` on its include path, so no project or filter changes. `git grep -l nlohmann -- '*.h' '*.cpp'` lists the two include sites plus 51 consumers.
  - API: nothing we use was removed or re-signatured. `parse` gained a defaulted `ignore_trailing_commas` (new `json.hpp:36051-36055`), so `DataPacker/Source/ExportJobs/ExportScene.cpp:194` is unchanged. `dump` is unchanged (`Tools/WorktreeCli/BuildCommand.cpp:621`). We use no deprecated API, and `InputFingerprint.cpp:277` `>>` is the current overload. Our tree has no `namespace nlohmann` forward declaration.
  - Pragmas: `_json` is now declared `operator""_json` (new `json.hpp:38448-38451`), so the C5311 pragma lines `ExternalHeaders.h:320-321,323` and `ToolCliCommon.h:21-22,24` are removed. If the build still reports C5311, they stay with a corrected comment.
  - Behavior: `dump()` of a binary64 now uses Zmij shortest digits instead of Grisu2 (new `json.hpp:27798-27888`); the text of a rare double can differ, but it round-trips to the same value. Parsing moves to correctly rounded Eisel-Lemire with a `strtod` fallback (new `json.hpp:9795`). Compile errors at the 51 consumers, such as the 3.11 comparison operators, are ported in our code.
- openexr RB-3.1 to main. The OpenEXRCore calls and `exr_decode_pipeline_t`/`exr_coding_channel_info_t` fields used at `DataPacker/Source/ExportJobs/Texture/Texture.cpp:146-215` are unchanged.
  - Wrapper file list: the list in `ThirdParty/Prebuilts/Source/DataPacker/openexr/openexr.c:6-33` changes, gaining `bytes.c`, `color_metadata.c`, `compression.c`, `debug.c`, `encoding.c`, `internal_b44_table_init.c`, `internal_dwa_table*.c`, `pack.c`, `std_attr.c`, and others.
  - Config shims: `openexr_config.h:10-12` now includes `OpenEXRConfig.h`, `IlmThreadConfig.h`, and `<Imath/ImathConfig.h>`, and `compression.c:18` includes `OpenEXRConfigInternal.h`. The shims are reworked: `Imath/ImathConfig.h` stays empty so Core uses its internal half conversion, `OpenEXRConfigInternal.h` sets `OPENEXR_USE_INTERNAL_DEFLATE 1`, and the version defines in `OpenEXRConfig.h` are reconciled with `openexr_version.h`. Any new shim file is registered through `/update-vcxproj`.
  - Deflate: ZIP decoding uses the libdeflate sources `compression.c:19-55` includes, so OpenEXR no longer uses zlib.
  - Stubs: our inputs are one-scanline chunks (`Texture.cpp:163` asserts `iScanlinesPerChunk == 1`: NONE, RLE, or ZIPS), so HT and zstd are not compiled. The wrapper defines `internal_exr_undo_ht`, `internal_exr_undo_zstd`, `internal_exr_apply_ht`, and `internal_exr_apply_zstd` (`compression.c:606,646-647,727-731` at `471f3c0b`), each returning `EXR_ERR_FEATURE_NOT_IMPLEMENTED`, with a one-line comment recording that decision.
- mimalloc v2 to v3 `dev3`.
  - The break: `mi_arena_id_t` became `void*` (`include/mimalloc.h:349`), so `GlobalAllocator.cpp:252` `mi_arena_area(1, ...)` no longer compiles, and v3 exposes no id for the startup arena.
  - The port: `MemoryInitializer` reads `mi_stats_get` once (`include/mimalloc-stats.h:165`). That read moves out of the `kbMimallocDiagnostics` block, which reuses it. The existing warning, now naming reserved and committed MiB, fires when `reserved.current` or `committed.current` is below `kiMimallocArenaReserveMebibytes`, or when `mi_stats_get` returns `false`.
  - Upstream evidence: v3 init calls `mi_reserve_os_memory(size, commit = true, ...)` (`src/heap/init.c:609-613`), and the OS layer counts that memory in the subprocess `reserved`/`committed` stats with no `MI_STAT` gate (`src/prim/os.c:338-340`, `include/mimalloc/util/stats.h:59`).
  - Unchanged: `mi_new*`, `mi_free*`, `mi_option_set`, `mi_register_output`, `mi_stats_get/merge`, and the `arena_count`/`committed`/`page_committed` fields (`GlobalAllocator.cpp:261-290`, `Engine/Source/Server/ServerDisplay.cpp:113-122`); `MI_STAT_VERSION` moves from 4 to 6. The `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp:6-8` defaults and `src/static.c` are also unchanged.
  - Operator fork: the hand-maintained fork (`Engine/Source/Memory/AGENTS.md`) is diffed. The stock header changed only whitespace and an aligned-new guard that selects the same branch under C++23.
  - Comments: `GlobalAllocator.cpp:250` and `Engine/Source/Memory/AGENTS.md:11` ("verifies arena 0") are rewritten.
- imgui 1.92.6 WIP to 1.93.1 WIP. The test-engine hooks are unchanged (`Engine/Source/Agent/AgentUiRegistry.cpp:282-308`, `AgentUiRegistryServerStubs.cpp:17-29`).
  - `AddRect`: the order is now `(rounding, thickness, flags)`, so `MenuUtils.cpp:310` changes from `(rounding, 0, thickness)`.
  - Samplers: the Vulkan backend allocates `IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE` (2) samplers from our pool (`Engine/Source/Graphics/Managers/ImGuiManager.cpp:127`), so the `VK_DESCRIPTOR_TYPE_SAMPLER` count at `DeviceManager.cpp:271` grows from 16 to 18, and the `:270` comment names them.
  - Fonts: `FontDataOwnedByAtlas = false` (`ImGuiManager.cpp:109`) now borrows the data until the atlas is destroyed. A code read confirms the eager font chunk outlives the context.
  - Accepted visual-only changes: stroke and anti-aliasing (`MenuUtils.cpp:302-349`), 1 px `Separator()`, and typed slider input (`MenuUtils.cpp:64`, `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:240,347`, `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:330`).
- implot: every function and `ImPlotSpec` field we use is unchanged (`Engine/Source/Profile/NetworkGraphs.cpp:15-20`, `Engine/Source/Ui/CurveWidget.cpp:29-122`). Its head targets imgui 1.93.0, and `implot_items.cpp:168-169` uses obsolete `ImDrawListFlags_AntiAliasedLines*` names; if that fails to build, see `## Decisions`.
- meshoptimizer 1.0 to 1.3: the five functions we call keep their signatures (`ExportScene.cpp:603,604,694`, `Scene/SceneVerticesLoader.cpp:201,204`, `Island/ProcessBakedRegion.cpp:248`). The new `remesher.cpp` and `tangentspace.cpp` stay out of `ThirdParty/Prebuilts/Source/DataPacker/meshoptimizer.cpp:5-23`.
- No API change at our sites:
  - SPIRV-Cross (`ExportShader.cpp:59-291`).
  - DirectXTK (wrappers `ThirdParty/Prebuilts/Source/Engine/DirectXTK.cpp:11-14`, `Source/DataPacker/DirectXTK.cpp:5`).
  - stb `stb_image_resize2.h` 2.17 to 2.18 (`Texture.cpp:284`, `Engine/Source/Graphics/Screenshot.cpp:134`).
  - zlib 1.3.2.1 (`Texture.cpp:609-612`, `ExportTexture.cpp:177`, `Engine/Source/File/PackChunkLoader.cpp:369`; file set `ThirdParty.vcxproj:170-180` still exists).
  - glm master, through gli only (`GLM_STATIC_ASSERT` is already defined at `ExternalHeaders.h:349`).
  - lz4 dev (`Engine/Source/Network/NetworkSerialization.cpp:347,365`, `PackChunkLoader.cpp:362`; `lz4hc.c:59-67` keeps the `LZ4_SRC_INCLUDED` reuse `ThirdParty/Prebuilts/Source/Engine/Lz4.cpp:6-9` relies on).
  - enet (same byte swap, no wire change).
  - bc7enc_rdo (README only); tinyobjloader (no consumer).
  - scb-check: `analyze_files` gained a defaulted keyword compatible with `.agents/skills/code-quality-metrics/scripts/Analyze-CodeQualityMetrics.py:287-323`, and the next metrics capture rebinds to the new pin.

## Determinism, CRC, and data exposure

- Simulation math does not change: Clipper2 (`Engine/Source/Frame/NavBuild.cpp`) and PerlinNoise (client-only, `Common/ExternalHeaders.h:265-268`) keep their pins, and glm is DataPacker-only.
- lz4 network batches (`NetworkSerialization.cpp:347`) are outside the CRC and keep a stable block format. Grid saves and replays use neither lz4 nor zlib nor nlohmann::json (`Engine/Source/File/AGENTS.md` `## Grid Saves`). enet has no wire change.
- mimalloc v3 changes only the allocation layout; any address-order dependence in the simulation would show up in the replay determinism check.
- No pack, chunk, or intermediate format changes. Producers whose bytes could change on re-export are:
  - tinygltf (now with nlohmann develop parsing)
  - meshoptimizer
  - stb_image_resize2 (mips)
  - zlib `compress2` intermediates
  - LZ4HC texture chunks
  - openexr (`Normals.exr` decode)
  - gli/glm (IBL pre-pass)
- Island chunks feed deterministic placement through the hull (`DataPacker/Source/ExportJobs/Island/AGENTS.md` `## Island Outputs`), so step 12 must prove their bytes unchanged.
- nlohmann::json's Zmij `dump()` can change the text of a fingerprint (`ExportScene.cpp:280,356`, `ExportIsland.cpp:501,535`, `ExportCubemapIbl.cpp:47,59`, `BakeRoute.cpp:91`, `InputFingerprint.cpp:210,314`) and so force a re-export, which step 12 does anyway. The Gaea bake fingerprint (`BakeRoute.cpp:73-81`) holds no doubles. `BakedDimensions.json` text (`Island/ProcessBakedRegion.cpp:291`) may change, but the value parsed back from it does not.

## License review

Every license file at the new heads was read. All match the old pins except zlib's year line, and `ThirdParty/AGENTS.md` `## License Policy` accepts each compiled set:

- openexr OpenEXRCore: BSD-3-Clause (`LICENSE.md`). Its `PATENTS` file offers the DWA files under BSD-3 plus a patent grant, or under Apache-2.0.
- `external/deflate/lib/**` (libdeflate, newly compiled): MIT (`external/deflate/COPYING`). OpenJPH (BSD-2) and zstd (BSD) are not compiled.
- tinygltf: we compile `attic/tiny_gltf.h` and the vendored `attic/stb_image.h`/`stb_image_write.h` under the top-level MIT `LICENSE`, which is unchanged from our pin. `attic/json.hpp` is no longer compiled.
- mimalloc v3 `src/**` through `static.c`: MIT.
- meshoptimizer: MIT.
- lz4 `lib/**`: BSD-2, with its GPL trees still unreferenced (`ThirdParty/AGENTS.md:33`).
- scb-check: Apache-2.0 `LICENSE` and MIT `pyproject.toml`, both unchanged.
- nlohmann/json, the new dependency (steps 1-4):
  1. `LICENSE.MIT` at `5683f5e0` is MIT, "Copyright (c) 2013-2026 Niels Lohmann".
  2. Only `single_include/nlohmann/json.hpp` is compiled, into the Engine, DataPacker, and AgentTools. Its SPDX headers and `REUSE.toml` give MIT for the library and its embedded Hoehrmann, Grisu2, Zmij, and fast_float-derived parts, CC0-1.0 for Hedley, and MIT AND Apache-2.0 for the Abseil-derived part. The repository's GPL-3.0 `tests/thirdparty/imapdl/**` and BSD-3 `tools/amalgamate/**` stay unreferenced.
  3. `Attribution.cpp` copies json's `README.md`, whose `## License` section carries the full MIT text and every embedded notice (`DataPacker/Source/Attribution.cpp:104-143`). Today json's notice is collected nowhere, so attribution improves with no code change.
  4. The user approved the dependency.
- Not collected, as before: the nested libdeflate notice. DataPacker copies only each library's top-level license (`Attribution.cpp:104-141,157-185`), and libdeflate is compiled only into DataPacker code paths that the game executables do not reference.

## In scope

- `.gitmodules`:
  - Every one of the 21 existing entries gains a tab-indented `branch = <name>` from `## Branch choices`; lz4's line changes to `dev`.
  - A new entry `[submodule "ThirdParty/json"]` (`path = ThirdParty/json`, `url = https://github.com/nlohmann/json.git`, `branch = develop`) is appended.
- Gitlinks: the 17 moved submodules plus new `ThirdParty/json` are set to the heads recorded in step 6. The four already at head keep theirs unless step 5 finds a newer head.
- `Common/ExternalHeaders.h`:
  - `:317-323`: the json include path and comment, and removal of the C5311 pragmas.
  - `:379-381`: `TINYGLTF_NO_INCLUDE_JSON`, the `tinygltf/attic/tiny_gltf.h` path, and the comment.
- `Tools/ToolCommon/ToolCliCommon.h:19-24`: the json include path and comment, and removal of the C5311 pragmas.
- `ThirdParty/Prebuilts/Source/DataPacker/tinygltf.cpp`: the define, the json include first, and the attic path.
- `ThirdParty/Prebuilts/Source/DataPacker/openexr/`: the `openexr.c` file list, the four HT/zstd stubs with their comment, and reworked or new config shims. Plus their `ThirdParty.vcxproj`/`.filters` registration and any include-path change in all three ThirdParty configurations, through `/update-vcxproj`.
- mimalloc: `Engine/Source/Memory/GlobalAllocator.cpp:250-271`, the reserve check through `mi_stats_get` and its comment, and the `Engine/Source/Memory/AGENTS.md:11` sentence that describes it.
- imgui: `Engine/Source/Ui/MenuUtils.cpp:310` and `Engine/Source/Graphics/Managers/DeviceManager.cpp:270-271`.
- `Documents/FreshMachineSetup.md` `## 3. Clone with symlinks and submodules`:
  after the clone, run the commands below so a fresh primary matches
  `## Decisions`. A setting change alone leaves an existing link on disk, and so
  does `git checkout -- <path>` (observed with Git 2.54.0.windows.1), so each
  link is removed and checked out again. The paths are the two submodules'
  mode-`120000` entries at the committed pins
  (`git -C ThirdParty/<lib> ls-files -s`), shown here for the
  `## Branch choices` heads.
  - `git -C ThirdParty/openexr config core.symlinks false`
  - `git -C ThirdParty/openexr rm -q -- src/test/OpenEXRFuzzTest/oss-fuzz`
  - `git -C ThirdParty/openexr checkout HEAD -- src/test/OpenEXRFuzzTest/oss-fuzz`
  - `git -C ThirdParty/tinygltf config core.symlinks false`
  - `git -C ThirdParty/tinygltf rm -q -- attic/examples/saver/Makefile`
  - `git -C ThirdParty/tinygltf checkout HEAD -- attic/examples/saver/Makefile`
- Compile-driven fixes in our own wrappers and consumers of any library above, including the 51 nlohmann consumers, limited to what the build requires.
- Verification only, never committed: the step-12 temporary version bumps and their cache entries, regenerated untracked data, and `Temp/ThirdPartyUpdate/`.
- Primary maintenance, authorized by this plan: steps 3, 18, and 19.

## Out of scope

- Any edit inside a submodule tree, including imgui or implot compatibility fixes (`ThirdParty/AGENTS.md:3`).
- The tinygltf v3 C API, a port to it, and any DataPacker glTF code change.
- Compiling OpenJPH, zstd, meshoptimizer `remesher.cpp`/`tangentspace.cpp`, tinyobjloader, or any file a wrapper does not need.
- Referencing any nlohmann/json file other than `single_include/nlohmann/json.hpp`, or adding any library other than nlohmann/json.
- `DataPacker/Source/Attribution.cpp` and `ThirdParty/AGENTS.md`.
- Changes to provisioning, bootstrap, landing, or compile tooling, including teaching provisioning to accept upstream symbolic links.
- Landing a DataPacker version bump or regenerated tracked output unless step 12 proves changed bytes and the user approves; any Gaea re-bake.
- Restoring imgui's pre-1.93 drawing exactly, slider live-edit behavior, or layout around the 1 px `Separator`.

## Invariants

- Upstream trees stay pristine. Adaptation stays in `ThirdParty/Prebuilts/Source/`, and consumption stays in `Common/ExternalHeaders.h` and `ToolCliCommon.h` (`ThirdParty/AGENTS.md` `## Adaptation and Consumption`).
- Only files accepted under `## License Policy` are compiled, and exactly one nlohmann::json version is compiled into any program.
- The mimalloc startup check still warns and continues when the reserve is missing.
- PostRender state stays bit-deterministic.
- No pack, chunk, or intermediate format changes; island chunk bytes are unchanged.
- Primary's Debug/Profile/Release ThirdParty libraries stay nonempty and track primary `HEAD` after step 19 (`ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md` `## Consumer Provisioning`).
- No other session runs between step 1 and step 19, and afterwards other sessions need only the documented rebase remedy.

## Acceptance criteria

- `git ls-files -s ThirdParty` in the commit lists 22 gitlinks, each equal to its `Temp/ThirdPartyUpdate/Pins.txt` record. The unchanged tree shows old pins for 17 of 21 and no `ThirdParty/json`.
- `git config -f .gitmodules --get-regexp '\.branch$'` returns 22 entries that match `## Branch choices`, plus any step-5 update. The unchanged tree returns one, `lz4 release`.
- `git grep -n -E 'tinygltf/json|5311' -- Common Tools` returns nothing, unless the build kept C5311 with a corrected comment.
- `git grep -n 'tinygltf/tiny_gltf.h' -- Common ThirdParty/Prebuilts` returns nothing, and both tiny_gltf include sites define `TINYGLTF_NO_INCLUDE_JSON`.
- `/compile` succeeds for ThirdParty Debug, Profile, and Release; WorktreeCli, AgentHarness, and DataPacker Release; and Client and Server Debug, Profile, and Release. The unchanged sources fail, for example at `GlobalAllocator.cpp:252`.
- After step 12, all 23 `Output/Data` files equal the step-3 snapshot and no tracked data file outside the `.PreExport` markers changed, or the user approved each difference.
- After step 13, `git diff` shows none of the temporary bumps and no `.PreExport` change.
- `/agent-harness` smoke run: client and server launch, connect, and render a frame with the UI and a scene model. An agent command round-trip succeeds, which is json parse and dump on the command channel. Both logs show no mimalloc reserve warning, and both executables quit cleanly.
- The `/agent-harness` replay determinism check passes with matching per-tick CRCs.
- A code read confirms the eager font chunk outlives the ImGui context (`ImGuiManager.cpp:109`).
- The DataPacker `Attribution/` output (`Attribution.cpp:202`) holds `json/README.md`.
- `git -C ThirdParty/<lib> status --porcelain` is empty for all 22 submodules before the commit.
- After step 6, `git -C ThirdParty/openexr config core.symlinks` and the same for tinygltf print `false`, the step-6 reparse-point scan prints nothing for all 22 submodule trees, `git -C ThirdParty/<lib> ls-files -s` still records both links as mode `120000`, and no other submodule records a mode-`120000` entry. At the unchanged pins no submodule tree has a mode-`120000` entry.
- `Documents/FreshMachineSetup.md` step 3 carries the `## In scope` commands for the paths the committed pins track.
- After step 19, primary `git status --porcelain` is empty, the three `ThirdParty.<Config>.lib` files are rebuilt, promotion reports success, and a linked worktree rebased onto the new tip passes provisioning.

## Review roles

- `implementer`: `/implement-plan`, `/update-affected-code`, `/update-claude-docs`.
- `mechanic`: `/code-style-review`, `/update-vcxproj`, `/comment-review`.
- `reviewer`: `/over-engineering-review` finder, with an `implementer` as validator; `/repo-code-review`; `/coherence-review`; `/adversarial-review`.
- `builder`: `/compile`.
