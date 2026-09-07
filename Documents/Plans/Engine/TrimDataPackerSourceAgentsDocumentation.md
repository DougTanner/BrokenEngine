<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:53:46.435Z","dependsOn":[]} -->
# Trim DataPacker source agent documentation

## Context

`DataPacker/Source/AGENTS.md` measures 3,017 `bt-token-v1` tokens against a 1,929-token formula budget (22,268 direct code tokens, one direct child document), an advisory excess of 1,088. The document mixes top-level orchestration and safety contracts with detailed cache, fingerprint, export-counting, and publication mechanics also owned by narrower code and `ExportJobs/AGENTS.md`.

## Design

The author recommends condensing the opening Architecture section by replacing narrative inventories with short ownership and failure-boundary rules, and removing repeated cache/export mechanics where the child document or existing compile documentation is already authoritative. Keep the trust-boundary, noninteractive-diagnostic, Gaea guard, copy-on-write, publication atomicity, and Local-versus-Shared data-mode contracts explicit because violating them can corrupt output or trigger expensive work. Collapse `## Design Patterns` and `## Output Structure` to the facts not already stated in Architecture, and keep direct links to narrower owners.

## Critical files

- `DataPacker/Source/AGENTS.md`

## In scope

- Condense `## Architecture`, especially the export-summary, fingerprint-cache, command-shape, and data-mode passages, by removing inventories, examples, repeated rationale, and mechanics owned by linked documentation.
- Merge repeated atomic-publication and generated-output statements across `## Architecture`, `## Design Patterns`, and `## Output Structure`.
- Shorten `## See Also` after preserving the governing child link.

## Out of scope

- C++, scripts, export formats, cache formats, `.pack`/manifest behavior, or build behavior.
- Removing operative safety, trust-boundary, Gaea authorization, diagnostic-mode, copy-on-write, or atomic-publication contracts.
- Splitting the document or changing another `AGENTS.md`.

## Acceptance criteria

- Remeasure the document with the repository `bt-token-v1` tooling and record its tokens and then-current formula budget.
- The document is at or below budget, or the completion evidence identifies the remaining operative rules that make an advisory excess unavoidable.
- Every preserved DataPacker contract remains discoverable in this document or through one direct authoritative link, without duplicated mechanics.

## Classification

Tier 1 — documentation-only condensation with no runtime, format, public signature, or invariant change.

## Coordination

None. This document can be trimmed independently of current executable Plans.

## Notes

The budget is advisory; do not delete an operative rule merely to meet the number.
