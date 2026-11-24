<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T01:21:41.541Z","dependsOn":[]} -->
# Clarify dump_render_target raw-output format rule in agent-harness command reference

## Context

During external skill validation (`VS001`) and coherence review of the
`MatchLightingAmbientToDirections` change, reviewers flagged that
`.agents/skills/agent-harness/references/command-reference.md:262-263` states:

`Single-channel output normalizes to grayscale PNG; four-byte RGBA writes direct PNG; float formats require raw .bin.`

Verified root cause in `Engine/Source/Graphics/Screenshot.cpp`:

- `IsFourByteColor` (`:341-345`) encodes `VK_FORMAT_R8G8B8A8_UNORM`,
  `VK_FORMAT_R8G8B8A8_SRGB`, `VK_FORMAT_B8G8R8A8_UNORM`, and
  `VK_FORMAT_B8G8R8A8_SRGB` directly to PNG (`EncodeAndWriteDump` `:428-449`).
- `IsSingleChannelNormalizable` (`:352-355`) normalizes `VK_FORMAT_R16_UNORM`,
  `VK_FORMAT_R16_SFLOAT`, and `VK_FORMAT_R32_SFLOAT` to min/max-scaled grayscale
  PNG (`EncodeAndWriteDump` `:450-481`).
- `ValidateDumpRenderTargetRequest` (`:503-507`) requires `raw:true` only when
  the target's format is neither `IsFourByteColor` nor
  `IsSingleChannelNormalizable` (for example multi-channel float formats such as
  `R16G16B16A16_SFLOAT`), throwing
  `"... is not PNG-encodable; pass raw:true to dump the raw texels"`.

The phrase `float formats require raw .bin` is therefore too broad:
single-channel float formats (`R16_SFLOAT` and `R32_SFLOAT`, such as
`TerrainElevation`) write grayscale PNG without `raw:true`, and only formats
without PNG support require `raw:true`.

Pre-existing proof: baseline commit `b8ed500a6f6a442978f12c9761cfe8ba5e3831af`
already contained the exact wording in
`.agents/skills/agent-harness/references/command-reference.md`; it was outside
the approved `AmbientHistory`-removal boundary of
`MatchLightingAmbientToDirections`.

## Design

Update the format-encoding sentence in the `dump_render_target` entry of
`.agents/skills/agent-harness/references/command-reference.md:262-263` to match
`Engine/Source/Graphics/Screenshot.cpp:341-355`, `:428-482`, and `:503-507`.

Recommended wording: state that single-channel normalizable formats
(`R16_UNORM`, `R16_SFLOAT`, `R32_SFLOAT`) normalize to grayscale PNG, supported
8-bit RGBA/BGRA UNORM/SRGB formats (`R8G8B8A8_UNORM`, `R8G8B8A8_SRGB`,
`B8G8R8A8_UNORM`, `B8G8R8A8_SRGB`) produce PNG, and formats without PNG support
(such as multi-channel float formats) require `raw:true` (writing `.bin`),
rewrapping only the edited bullet paragraph to ~80 columns. Rationale:
accurately reflects `ValidateDumpRenderTargetRequest` without changing runtime
behavior or duplicating implementation details.

Change Workflow tier: Tier 1 — trigger: mechanical documentation correction in
one skill reference file, with no C++, shader, determinism/CRC, serialization,
replay, wire, affinity, threading, allocation, or build exposure.

## Critical files

- `.agents/skills/agent-harness/references/command-reference.md` —
  `dump_render_target` bullet (`:257-264`), file to edit.
- `Engine/Source/Graphics/Screenshot.cpp` — `IsFourByteColor` (`:341-345`),
  `IsSingleChannelNormalizable` (`:352-355`), `EncodeAndWriteDump` (`:428-482`),
  and `ValidateDumpRenderTargetRequest` (`:493-508`), read-only source of truth.

## In scope

- Updating the PNG/raw-format wording in the `dump_render_target` entry of
  `.agents/skills/agent-harness/references/command-reference.md:262-263` and
  rewrapping that paragraph.

## Out of scope

- Any edits to `Engine/Source/Graphics/Screenshot.cpp` or runtime
  `dump_render_target` behavior.
- Other commands in `.agents/skills/agent-harness/references/command-reference.md`
  or other files under `.agents/skills/agent-harness/`.
- Render-target index or name changes owned by other Plans.

## Acceptance criteria

- `.agents/skills/agent-harness/references/command-reference.md` accurately
  states that single-channel normalizable formats (including `R16_SFLOAT` and
  `R32_SFLOAT`) produce grayscale PNG without `raw:true`, supported 8-bit
  RGBA/BGRA UNORM/SRGB formats produce PNG, and formats without PNG support
  require `raw:true`, matching `Engine/Source/Graphics/Screenshot.cpp:341-355`,
  `:432-436`, and `:503-507`.
- `pwsh -NoProfile -NonInteractive -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/agent-harness`
  exits 0 (`VALID`).
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every row
  the change triggers passing.

## Notes

Ordinary pre-existing documentation debt; no session provenance block applies.
No C++ build or live `/agent-harness` verification is required.
