<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T02:20:33.641Z","dependsOn":[]} -->
# Classify every dual-language shader layout header as dual-language

## Context

`/repo-code-review` selects its C++ targets from the change inventory:
`.agents/skills/repo-code-review/SKILL.md:31-32` states that the `cpp` and
`dual-language-header` classes "are the only statement of which `.h` files are
GLSL-only, and every `dual-language-header` entry routes" into the C++ review.

That classification lives in `.agents/scripts/Get-SessionChangeInventory.ps1`.
`Get-PathClass` (`:164-187`) recognizes a dual-language header only by the
hardcoded leaf allowlist `$script:DualLanguageHeaders = @('shaderlayouts.h',
'shaderlayoutsbase.h')` (`:29`, tested at `:170`); every other `.h` under a
`Data/Shaders` directory falls through to `:178` and is classified `glsl`.

`Engine/Data/Shaders/ShaderGlobalLayout.h` is compiled as C++. The chain is
`Projects/BrokenEngineSandbox/Source/Pch.h:100` →
`Projects/BrokenEngineSandbox/Data/Shaders/ShaderLayouts.h:5` →
`Engine/Data/Shaders/ShaderLayoutsBase.h:192,194`, which includes
`ShaderGlobalLayout.h` and `ShaderMainLayout.h` inside the `BT_ENGINE` branch
that defines the `INIT`, `CONSTEXPR`, and vector-type bridge macros
(`ShaderLayoutsBase.h:10-17`). Those two headers therefore declare real C++
structs, but the allowlist does not name them, so they are classified `glsl` and
never reach the C++ targets file.

Observed this session: a change to `Engine/Data/Shaders/ShaderGlobalLayout.h` was
missing from the `/repo-code-review` targets file, the C++ reviewer blocked on
the omission, and the following round had to accept compensating evidence
instead. The two other layout headers reachable from the same chain would fail
the same way.

## Design

Extend the existing mechanism rather than adding a second one: add
`shadergloballayout.h` and `shadermainlayout.h` to `$script:DualLanguageHeaders`,
and replace the list's comment with one naming the include chain that defines the
set — the C++ side reaches exactly `ShaderLayouts.h`, `ShaderLayoutsBase.h`, and
the headers `ShaderLayoutsBase.h` includes — so a future layout header added to
that chain is added here too. Both classes already route into `/glsl-review`
(`.agents/skills/glsl-review/SKILL.md:24`), so the reclassified headers keep
their shader review and gain the C++ review they were missing.

Every other `.h` under `Data/Shaders` (`ShaderFunctions.h`, `ShaderRandom.h`,
`ModelCommon.h`, the `Water/`, `Wind/`, and `Smoke/` helpers) is GLSL-only —
none is reachable from a C++ translation unit — and stays classified `glsl`.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1` — `$script:DualLanguageHeaders`
  (`:27-29`) and its comment
- `.agents/scripts/Test-SessionChangeInventoryFixtures.ps1` — the classification
  fixture rows (`:189-190,213`)

## In scope

- The `$script:DualLanguageHeaders` entries and the comment above them
- Fixture rows and counts covering a changed `ShaderGlobalLayout.h` classified
  `dual-language-header`, and a GLSL-only `Data/Shaders` header still classified
  `glsl`

## Out of scope

- `Get-PathClass`'s other rules, the `Data/Shaders` directory clause, and the
  shader extension list
- The `triggers`, `regions`, `landing`, and truncation sections of the inventory
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`, which consumes
  the classification and performs none of its own
- `repo-code-review`, `glsl-review`, and `code-style-review` skill documents,
  which already defer to these class rules and must not restate them

## Risk tier and invariants

Expected Change Workflow Tier 2 — scoped tool behavior in one script, with no
runtime, determinism, CRC, wire, or build-coordination surface. Invariants: the
class list and JSON envelope shape are unchanged; a reclassified header must
appear in both the C++ and the GLSL selections, never dropping out of the shader
review.

## Acceptance criteria

- With `Engine/Data/Shaders/ShaderGlobalLayout.h` changed, the inventory reports
  it as `dual-language-header`, and a `/repo-code-review` targets file built from
  that inventory contains it
- `Engine/Data/Shaders/ShaderFunctions.h` still classifies as `glsl`
- `pwsh -NoProfile -File .agents/scripts/Test-SessionChangeInventoryFixtures.ps1`
  passes
