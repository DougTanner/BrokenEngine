<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:12:57.899Z","dependsOn":[]} -->
# Store the RenderDoc capture baseline as int64_t

## Context

`Documents/C++StyleGuide.txt` rule 13 (lines 101-106) has our own members use `int64_t` even when the value comes from another API, converting only where it is passed to that API. `RenderDocCaptureState::uiBaselineCaptures` (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:106`) stores RenderDoc's `GetNumCaptures()` result as `uint32_t`. Origin: `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md` (entry `AgentCommandsClientGeneric` F2), deleted when the Plans are created. The sweep kept the type because `baseline + frames` wraps modulo 2^32 today.

Verified current state:

- Assigned from `pRenderDocApi->GetNumCaptures()` at `AgentCommandsClientGeneric.cpp:293` and `:310`.
- Compared at `:327` as `GetNumCaptures() < uiBaselineCaptures + static_cast<uint32_t>(iFrames)`.
- Iterated at `:357` with an `int64_t i` from `uiBaselineCaptures` to `uiBaselineCaptures + static_cast<uint32_t>(iFrames)`, then passed to `GetCapture(static_cast<uint32_t>(i), ...)` at `:361` and `:366`.
- The wrap is unreachable: `iFrames` is validated to `[1, 8]` at `:266-267`, and the baseline counts captures RenderDoc has written to disk in this process, which cannot approach 2^32.

## Design

The author recommends `int64_t iBaselineCaptures = 0;` at `:106`, assigned with `static_cast<int64_t>(pRenderDocApi->GetNumCaptures())` at `:293` and `:310`; `:327` becomes `static_cast<int64_t>(pRenderDocApi->GetNumCaptures()) < pState->iBaselineCaptures + pState->iFrames`; `:357` loops `for (int64_t i = pState->iBaselineCaptures; i < pState->iBaselineCaptures + pState->iFrames; ++i)`. The existing `static_cast<uint32_t>(i)` at the `GetCapture` calls stays: it is the API boundary rule 13 names.

Rationale: the value converts once at each API edge, and the two `static_cast<uint32_t>(iFrames)` casts disappear. No reachable value changes, so the command's completion condition and returned capture paths are unchanged.

Risk tier: Tier 1 — `.agents/references/risk-tiers.md`; trigger: local behavior-preserving type change inside one file-local struct, with no public signature, wire, CRC, or threading exposure.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`

## In scope

- `RenderDocCaptureState::uiBaselineCaptures` (`:106`) and its uses at `:293`, `:310`, `:327`, `:357`.

## Out of scope

- `FrameCountParameter` and its unsigned JSON range check (`:24-53`), `CommandMouse`'s unsigned wheel-notch range check (`:1036`), and every other RenderDoc API call and cast.
- The `renderdoc_capture` command's parameters, phases, and JSON result.

## Notes

- Verification: compile the client (`/compile`); `/agent-harness` runs `renderdoc_capture` with `frames` 1 and 3 under RenderDoc (`.agents/skills/agent-harness/references/renderdoc.md`) and confirms one and three capture paths are returned.
- The investigation's other `AgentCommandsClientGeneric` entries (F1 at `:40`, F13 at `:1036`: unsigned JSON values range-checked before conversion) keep their types under the full-range unsigned arithmetic bullet of rule 13 in `Documents/C++StyleGuide.txt`.
