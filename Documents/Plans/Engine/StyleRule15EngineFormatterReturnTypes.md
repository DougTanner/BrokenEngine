<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:06:11.317Z","dependsOn":[]} -->
# Cleanup: Engine — spell the return type of the four Engine.h std::formatter::format members (style rule 15)

## Context
`Documents/C++StyleGuide.txt` rule 15 forbids `auto` outside its listed cases,
and a deduced function return type is not one of them. The four
`std::formatter` specializations in `Engine/Source/Engine.h` declare
`auto format(..., CONTEXT& rContext) const` at `:111` (`engine::alignment_t`),
`:121` (`engine::Alignments`), `:139` (`engine::uuid_t`) and `:149`
(`engine::id_t<T>`). Each returns the base formatter's `format` result, whose
standard declared type is `typename FormatContext::iterator`, so one exact
type exists for each: `typename CONTEXT::iterator`.

The Engine scanner-rule sweep left these four as residuals so they would stay
parallel with the `auto format` formatters in `Common/Log/LogFormatters.h` and
`Common/Workbuffer.h`. `Engine/` already spells the type in its other
formatter, `std::formatter<engine::global_id_t>`
(`Engine/Source/Frame/Collections/CollectionId.h:116`), so `Engine/` is already
mixed. This change makes `Engine.h` match `CollectionId.h`.

## Design
The author's recommendation: replace `auto` with `typename CONTEXT::iterator`
in the four declarations and change nothing else. The deduced type and the
spelled type are the same, so overload resolution and every call are
unchanged.

## Critical files
- `Engine/Source/Engine.h`

## In scope
- The `format` member declarations at `Engine/Source/Engine.h:111`, `:121`,
  `:139` and `:149`.

## Out of scope
- The `auto format` formatters in `Common/Log/LogFormatters.h` and
  `Common/Workbuffer.h`. They belong to
  `Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`.
- The `auto` rule 15 rows the sweep recorded for generic-lambda parameters,
  parameter packs and deducing-`this` (`this auto&&`) parameters. None of
  these has a single exact type, so they are not violations.
- Every other change to the formatters' bodies.

## Risk tier and invariants
Tier 1 (mechanical): trigger is local behavior-preserving style work
(`.agents/references/risk-tiers.md`). The resolved signature does not change,
so no public signature is exposed. Log text is unchanged. No simulation, CRC,
serialization or wire exposure.

## Acceptance criteria
- `git grep -n "auto format(" -- Engine` returns no lines.
- `/compile` Client and Server Debug and Release builds pass.

## Notes
Originating residual: the Engine scanner-rule style sweep (rule 15 rows for
`Engine/Source/Engine.h`).
