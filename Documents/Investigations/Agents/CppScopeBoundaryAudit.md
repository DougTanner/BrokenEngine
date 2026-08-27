# C++ Scope Boundary Audit

## Context

The abandoned monolithic C++ review preparation was redirected into seven independently executable audit Plans. This seven-Plan suite is a one-time migration audit because the review-gate skills and agents were added recently and this pre-existing codebase has never passed through them; execution is a quick scan for clear violations of the new review rules. This Plan owns only current documented ownership boundaries over the first-party C++ corpus. At execution, freeze one clean Git SHA and use the authoritative classifier rules from `.agents/scripts/Get-SessionChangeInventory.ps1` against `git ls-tree -r <SHA>` to enumerate first-party C++ targets under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`, excluding `ThirdParty/` and generated paths. Current evidence is 529 C++ targets and exactly four dual-language headers. The audit is findings-only and does not change source or add capability.

## Design

1. Freeze the SHA, classifier, target paths, blobs, applicable authority chains, and live hashes. Read every applicable `AGENTS.md`, architecture document, and governing review directive from that frozen tree. Shard deterministically by subsystem and authority at no more than 80,000 `bt-token-v1`, splitting at file or authority boundaries as needed.
2. Start one fresh Luna `researcher` per shard, with fork none and no children. Each researcher owns a unique ignored report at `Temp/CppScopeBoundaryAudit/<sha>/<shard>.md`, covers every assigned file, and may read callers, consumers, producers, and mirrors outside the shard while keeping the candidate anchor/root in the owned shard. Reports assign deterministic candidate IDs (recommended form `CSB/<shard>/<###>`, with the frozen SHA in report metadata) and record severity `LOW`/`MEDIUM`/`HIGH`/`CRITICAL`, confidence, occurrence `observed`/`credible_exposure`/`hypothetical`, path:line and symbol, controlling directive or review concept, claim, evidence, reachability, refutation, impact, next step, and any external claim. Hypothetical candidates are at most MEDIUM; CRITICAL is catastrophic/core; HIGH is major and credible or observed; MEDIUM is localized; LOW is weak or cosmetic.
3. A Luna consolidator verifies exact manifest coverage, no missing or duplicate target, and no baseline/hash mismatch, then writes one ignored Temp index without dropping semantics. A fresh Sol triages every candidate; LOW/MEDIUM are default drop but remain promotable. Sol tries to disprove every retained or promoted HIGH/CRITICAL against frozen source, authorities, callers, guards, and lifecycle. Send each atomic external claim through one Luna `/verify-external-claims` locator. An existing compile builder or `agent-harness` implementer is optional and used only when directly decisive. Do not instrument or edit source; absent decisive evidence remains unresolved.
4. Compare current implementation responsibilities and public contracts only with documented subsystem, layer, client/server, CPU/GPU, and explicit ownership or affinity boundaries. A candidate must identify the controlling authority and a concrete placement, ownership, affinity, or responsibility violation, including the consumer or writer that crosses the boundary. Do not infer a boundary from naming alone; read the owner chain and relevant callers.
5. Refuted or final-advisory candidates create no tracked record. A serious, decision-complete candidate with a proven root cause, boundary, invariants, and acceptance signal routes through `/create-follow-up-plans`; a serious candidate with open intended behavior, cause, correction, external fact, or decisive check routes to the owning area under `Documents/Investigations/`. Exact duplicates map or update existing records only under repository rules. No source fix or capability work is authorized.

## Critical files

- `AGENTS.md`, `Documents/AGENTS.md`, `Documents/Plans/AGENTS.md`, all applicable nested `AGENTS.md`, and `Documents/Architecture/` authorities
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/skills/scope-review/SKILL.md` and `.agents/skills/create-follow-up-plans/SKILL.md`
- First-party C++ targets and their callers, consumers, producers, mirrors, and authority documents under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`

## In scope

- Freezing and hashing the first-party C++ corpus and applicable authority chains described above; the six C++ audit lenses each cover all 529 frozen targets exactly once, including dual-language targets.
- Comparing current responsibilities and contracts with documented subsystem, layer, client/server, CPU/GPU, and explicit ownership or affinity boundaries for the scope-boundary lens.
- Reporting only concrete placement/ownership/affinity violations or responsibilities outside their documented owner, with the controlling boundary and affected consumer/writer.
- Deterministic shard manifests, Luna reports, the single Temp index, Sol triage and disproof, atomic external-claim verification, and durable routing of serious survivors.
- Creating or updating only the exact area-owned Plan or Investigation selected by the repository duplicate rules for a serious survivor; no advisory is tracked.

## Out of scope

- Historical change authorization, session-diff scope, or Plan `## In scope`/`## Out of scope` reconciliation; this is a current-code boundary audit only.
- KISS, minimality, worth/cost, overbuild, representation fit, or speculative machinery (CppPlanSimplicityAudit)
- Directive-to-implementation/consumer traceability, generic invariant correctness, and check adequacy (CppPlanTraceAudit and CppDirectiveVerificationAudit)
- Reachable runtime/integration invariant falsification as a standalone lens (CppAdversarialInvariantAudit)
- Comment/doc fact placement and wording/style (CppProgressiveDisclosureAudit)
- The dedicated four-header CPU/GLSL layout and binding audit (CppSharedHeaderGlslAudit)
- Source fixes, instrumentation, capability additions, unit tests, style cleanup, metrics findings, generic `/repo-code-review`, and any generated output outside this Plan's ignored Temp directory

## Risk tier and invariants

Plan-file authoring is Tier 1 documentation. Executing this full audit is Tier 3 because its corpus and possible durable survivors span independently owned subsystems and may add scheduler-visible Plans. Freeze a clean SHA; if any target, authority, or classifier hash changes, stop and restart at a new freeze rather than mixing baselines. Every target is covered exactly once for this lens, every candidate receives one Sol disposition, every serious candidate receives one deep verdict and one durable route, and no advisory is tracked. Boundary claims require a controlling documented owner; naming or intuition alone is insufficient. No source, shader, script, skill, or instruction bytes change during the audit. The future execution uses normal review, verification, and landing gates for any tracked output.

## Acceptance criteria

- The frozen classifier reproduces 529 first-party C++ targets and four dual-language headers, with live hashes equal to frozen hashes and no excluded or generated path included.
- Every applicable authority chain is read and every target appears exactly once in the scope-boundary shard coverage; every shard is at most 80,000 `bt-token-v1` or is split at a valid boundary.
- Every shard has one fresh Luna report and the consolidator's Temp index proves exact coverage with no missing, duplicate, or baseline-mismatched row; each candidate names a documented boundary, concrete owner/placement/affinity evidence, and a reachable affected responsibility.
- Every candidate has one Sol disposition; every retained or promoted serious candidate has a disproof attempt against source, authorities, callers, guards, and lifecycle, plus one locator verdict for each atomic external claim.
- Refuted/advisory candidates have no tracked record; every serious survivor has exactly one Plan or Investigation route with decision-complete evidence or an exact duplicate mapping, and every resulting Plan validates.
- Frozen hashes remain unchanged, all seven audit-lens boundaries remain separate, no source/GLSL/script/skill/instruction diff is introduced, and no build or unit test is claimed.

## Notes

This Plan has no dependency and is independently executable. The current evidence is a preparation signal, not a substitute for the execution freeze. The Plan creation stage itself requires the landing gate; future audit output follows the normal Tier-3 review, verification, and landing workflow.
