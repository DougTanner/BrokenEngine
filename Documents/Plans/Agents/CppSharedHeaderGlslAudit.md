<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T01:28:28.683Z","dependsOn":[]} -->
# C++ Shared Header GLSL Audit

## Context

The abandoned monolithic C++ review preparation was redirected into seven independently executable audit Plans. This Plan owns only the four authoritative dual-language CPU/GLSL headers and their layout, binding, upload, and dependency contracts. At execution, freeze one clean Git SHA and use the authoritative classifier rules from `.agents/scripts/Get-SessionChangeInventory.ps1` against `git ls-tree -r <SHA>` to identify the first-party C++ corpus and the dual-language set. Current evidence is 529 C++ targets and exactly these four dual-language headers: `Projects/BrokenEngineSandbox/Data/Shaders/ShaderLayouts.h`, `Engine/Data/Shaders/ShaderLayoutsBase.h`, `Engine/Data/Shaders/ShaderGlobalLayout.h`, and `Engine/Data/Shaders/ShaderMainLayout.h`. GLSL-only shader headers are evidence only, not anchors. This audit is findings-only and does not change source or add capability.

## Design

1. Freeze the SHA, classifier, four-header paths, blobs, applicable authority chains, shader include/dependency evidence, and live hashes. Read every applicable `AGENTS.md`, architecture document, and governing shader/CPU directive from that frozen tree. Make one deterministic bounded shard per authoritative header (split evidence as needed at no more than 80,000 `bt-token-v1`); cover the four headers exactly once, not the 529 C++ targets.
2. Start one fresh Luna `researcher` per header shard, with fork none and no children. Each researcher owns a unique ignored report at `Temp/CppSharedHeaderGlslAudit/<sha>/<shard>.md`, covers the assigned header, and may read all shader entry points and C++ callers/consumers outside the shard while keeping the candidate anchor/root in the owned header. Reports assign deterministic candidate IDs (recommended form `CSG/<shard>/<###>`, with the frozen SHA in report metadata) and record severity `LOW`/`MEDIUM`/`HIGH`/`CRITICAL`, confidence, occurrence `observed`/`credible_exposure`/`hypothetical`, path:line and symbol, controlling directive or review concept, claim, evidence, reachability, refutation, impact, next step, and any external claim. Hypothetical candidates are at most MEDIUM; CRITICAL is catastrophic/core; HIGH is major and credible or observed; MEDIUM is localized; LOW is weak or cosmetic.
3. A Luna consolidator verifies exact four-header coverage, no missing or duplicate header, and no baseline/hash mismatch, then writes one ignored Temp index without dropping semantics. A fresh Sol triages every candidate; LOW/MEDIUM are default drop but remain promotable. Sol tries to disprove every retained or promoted HIGH/CRITICAL against frozen headers, shader entry points, C++ upload/layout code, descriptor creation/writes, static assertions, and include/dependency reachability. Send each atomic external claim through one Luna `/verify-external-claims` locator. An existing compile builder or `agent-harness` implementer is optional and used only when directly decisive. Do not instrument or edit source; absent decisive evidence remains unresolved.
4. Trace both language surfaces: field widths/order/strides/qualifiers and block layout; descriptor and binding roles; C++ upload sizes, layout creation, writes, and static assertions; shader entry points that transitively include each header; and DataPacker dependency capture from preprocessing through dependency fingerprinting. Use the repository scalar-layout contract and actual include graph. GLSL-only headers may support evidence but cannot become candidate anchors or expand coverage beyond the four authoritative headers.
5. Refuted or final-advisory candidates create no tracked record. A serious, decision-complete candidate with a proven root cause, boundary, invariants, and acceptance signal routes through `/create-follow-up-plans`; a serious candidate with open intended behavior, cause, correction, external fact, or decisive check routes to the owning area under `Documents/Investigations/`. Exact duplicates map or update existing records only under repository rules. No source fix or capability work is authorized.

## Critical files

- `AGENTS.md`, `Documents/AGENTS.md`, `Documents/Plans/AGENTS.md`, all applicable nested `AGENTS.md`, `Documents/Architecture/`, and shader/graphics authority documents
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/skills/glsl-review/SKILL.md`, `.agents/skills/glsl-review/references/shader-footguns.md`, and `.agents/skills/create-follow-up-plans/SKILL.md`
- `Projects/BrokenEngineSandbox/Data/Shaders/ShaderLayouts.h`
- `Engine/Data/Shaders/ShaderLayoutsBase.h`, `Engine/Data/Shaders/ShaderGlobalLayout.h`, and `Engine/Data/Shaders/ShaderMainLayout.h`
- C++ shader-layout wrappers, upload/layout creation and static assertions, shader entry points, and DataPacker dependency inputs reached by those four headers

## In scope

- Freezing and hashing the classifier evidence and exactly the four authoritative dual-language headers named above; GLSL-only shader headers are evidence only and the 529 C++ targets are not this Plan's coverage set.
- Tracing CPU/GLSL field widths, order, strides, qualifiers, scalar-layout blocks, descriptor/binding roles, C++ uploads/static assertions, shader include/dependency reachability, and DataPacker dependency capture for each header.
- Deterministic four-header shard manifests, Luna reports, the single Temp index, Sol triage and disproof, atomic external-claim verification, and durable routing of serious survivors.
- Creating or updating only the exact area-owned Plan or Investigation selected by the repository duplicate rules for a serious survivor; no advisory is tracked.

## Out of scope

- Any GLSL-only header or shader as an audit anchor, and any generic 529-target C++ sweep
- Plan/design traceability, simplicity/KISS, current ownership boundaries, reachable runtime invariant falsification, progressive-disclosure comments, or directive/check adequacy as independent lenses (the other named audit Plans)
- Shader feature additions, shader algorithm redesign, performance tuning without directly decisive evidence, source fixes, instrumentation, capability additions, unit tests, style cleanup, metrics findings, generic `/repo-code-review`, and any generated output outside this Plan's ignored Temp directory

## Risk tier and invariants

Plan-file authoring is Tier 1 documentation. Executing this audit is Tier 3 because CPU/GLSL layout and binding contracts cross independently owned engine, project, shader, and DataPacker surfaces. Freeze a clean SHA; if any header, shader evidence, authority, classifier, or dependency hash changes, stop and restart at a new freeze rather than mixing baselines. Each of the four authoritative headers is covered exactly once, both language representations agree under the actual scalar-layout contract, descriptor roles remain consistent, and every candidate receives one Sol disposition, serious candidate one deep verdict and one durable route, and no advisory is tracked. No source, shader, script, skill, or instruction bytes change during the audit. The future execution uses normal review, verification, and landing gates for any tracked output.

## Acceptance criteria

- The frozen classifier confirms the 529-target corpus and exactly four authoritative dual-language headers, with all four header hashes and required authority/dependency hashes equal to the frozen values; GLSL-only headers are never anchors.
- Each of the four headers appears exactly once in bounded shard coverage; every shard is at most 80,000 `bt-token-v1` or is split at a valid evidence boundary.
- Every report covers its whole header and records exact CPU/GLSL field layout, qualifiers, binding roles, upload/static-assert evidence, include reachability, and DataPacker dependency evidence, with required candidate fields and severity/occurrence rules.
- Every candidate has one Sol disposition; every retained or promoted serious candidate has a disproof attempt across both language surfaces and one locator verdict for each atomic external claim.
- Refuted/advisory candidates have no tracked record; every serious survivor has exactly one Plan or Investigation route with decision-complete evidence or an exact duplicate mapping, and every resulting Plan validates.
- Frozen hashes remain unchanged, the four-header lens remains separate from the six 529-target lenses, no source/GLSL/script/skill/instruction diff is introduced, and no build or unit test is claimed.

## Notes

This Plan has no dependency and is independently executable. The current evidence is a preparation signal, not a substitute for the execution freeze. The Plan creation stage itself requires the landing gate; future audit output follows the normal Tier-3 review, verification, and landing workflow.
