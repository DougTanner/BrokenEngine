<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T01:28:24.100Z","dependsOn":[]} -->
# C++ Directive Verification Audit

## Context

The abandoned monolithic C++ review preparation was redirected into seven independently executable audit Plans. This Plan owns only decisive evidence and check adequacy for applicable directives and invariants over the first-party C++ corpus. At execution, freeze one clean Git SHA and use the authoritative classifier rules from `.agents/scripts/Get-SessionChangeInventory.ps1` against `git ls-tree -r <SHA>` to enumerate first-party C++ targets under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`, excluding `ThirdParty/` and generated paths. Current evidence is 529 C++ targets and exactly four dual-language headers. The audit is findings-only and does not change source or add capability.

## Design

1. Freeze the SHA, classifier, target paths, blobs, applicable authority chains, and live hashes. Read every applicable `AGENTS.md`, architecture document, and governing review directive from that frozen tree. Shard deterministically by subsystem and authority at no more than 80,000 `bt-token-v1`, splitting at file or authority boundaries as needed.
2. Start one fresh Luna `researcher` per shard, with fork none and no children. Each researcher owns a unique ignored report at `Temp/CppDirectiveVerificationAudit/<sha>/<shard>.md`, covers every assigned file, and may read callers, consumers, producers, and mirrors outside the shard while keeping the candidate anchor/root in the owned shard. Reports assign deterministic candidate IDs (recommended form `CDV/<shard>/<###>`, with the frozen SHA in report metadata) and record severity `LOW`/`MEDIUM`/`HIGH`/`CRITICAL`, confidence, occurrence `observed`/`credible_exposure`/`hypothetical`, path:line and symbol, controlling directive or review concept, concrete claim, evidence, reachability, refutation, impact, next step, and any external claim. Hypothetical candidates are at most MEDIUM; CRITICAL is catastrophic/core; HIGH is major and credible or observed; MEDIUM is localized; LOW is weak or cosmetic.
3. A Luna consolidator verifies exact manifest coverage, no missing or duplicate target, and no baseline/hash mismatch, then creates one ignored Temp index without dropping semantics. Build a baseline-qualified directive/invariant catalog and applicability ledger. For exactly one row per applicable item, record `PASS`, `FAIL`, `BLOCKED`, or `UNVERIFIED` with decisive evidence and the controlling source. Every `FAIL` or `UNVERIFIED` becomes a candidate. A `BLOCKED` row is rerun after the missing evidence is addressed; if it remains open, preserve it as an unresolved serious check when its impact warrants routing.
4. A fresh Sol triages every candidate; LOW/MEDIUM are default drop but remain promotable. Sol tries to disprove every retained or promoted HIGH/CRITICAL against frozen source, authorities, callers, guards, and lifecycle. Send each atomic external claim through one Luna `/verify-external-claims` locator. An existing compile builder or `agent-harness` implementer is optional and used only when directly decisive. Do not instrument or edit source; absent decisive evidence remains unresolved. This Plan owns evidence/check adequacy, not directive-to-implementation traceability.
5. Refuted or final-advisory candidates create no tracked record. A serious, decision-complete candidate with a proven root cause, boundary, invariants, and acceptance signal routes through `/create-follow-up-plans`; a serious candidate with open intended behavior, cause, correction, external fact, or decisive check routes to the owning area under `Documents/Investigations/`. Exact duplicates map or update existing records only under repository rules. No source fix or capability work is authorized.

## Critical files

- `AGENTS.md`, `Documents/AGENTS.md`, `Documents/Plans/AGENTS.md`, all applicable nested `AGENTS.md`, and `Documents/Architecture/` authorities
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/skills/plan-audit/SKILL.md`, `.agents/skills/create-follow-up-plans/SKILL.md`, and the repository's decisive validation/check mechanisms named by each authority
- First-party C++ targets and their callers, consumers, producers, mirrors, and authority documents under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`

## In scope

- Freezing and hashing the first-party C++ corpus and applicable authority chains described above; the six C++ audit lenses each cover all 529 frozen targets exactly once, including dual-language targets.
- Building the baseline-qualified directive/invariant catalog and applicability ledger, with exactly one `PASS`/`FAIL`/`BLOCKED`/`UNVERIFIED` row per applicable item.
- Owning decisive evidence and check adequacy: every `FAIL` and `UNVERIFIED` becomes a candidate, and every `BLOCKED` is rerun with its missing evidence tracked.
- Deterministic shard manifests, Luna reports, the single Temp index, Sol triage and disproof, atomic external-claim verification, and durable routing of serious survivors.
- Creating or updating only the exact area-owned Plan or Investigation selected by the repository duplicate rules for a serious survivor; no advisory is tracked.

## Out of scope

- Directive-to-implementation/current-consumer/mirror traceability, hidden defaults, contradictions, or orphan surfaces (CppPlanTraceAudit)
- KISS, worth/cost, simpler mechanisms, root-cause-versus-bandaid, actual-consumer benefit, or representation fit (CppPlanSimplicityAudit)
- Current documented subsystem/layer/client-server/CPU-GPU ownership or placement boundaries (CppScopeBoundaryAudit)
- Standalone reachable runtime/integration falsification (CppAdversarialInvariantAudit)
- Comment/doc fact placement and wording/style (CppProgressiveDisclosureAudit)
- The dedicated four-header CPU/GLSL layout and binding audit (CppSharedHeaderGlslAudit)
- Source fixes, instrumentation, capability additions, unit tests, style cleanup, metrics findings, generic `/repo-code-review`, and any generated output outside this Plan's ignored Temp directory

## Risk tier and invariants

Plan-file authoring is Tier 1 documentation. Executing this full audit is Tier 3 because its corpus and possible durable survivors span independently owned subsystems and may add scheduler-visible Plans. Freeze a clean SHA; if any target, authority, or classifier hash changes, stop and restart at a new freeze rather than mixing baselines. Every target is covered exactly once for this lens, every applicable item has exactly one ledger row, every candidate receives one Sol disposition, every serious candidate receives one deep verdict and one durable route, and no advisory is tracked. A BLOCKED check is never silently treated as PASS; it is rerun or remains explicitly unresolved. No source, shader, script, skill, or instruction bytes change during the audit. The future execution uses normal review, verification, and landing gates for any tracked output.

## Acceptance criteria

- The frozen classifier reproduces 529 first-party C++ targets and four dual-language headers, with live hashes equal to frozen hashes and no excluded or generated path included.
- Every applicable authority chain is read and every target appears exactly once in the verification shard coverage; every shard is at most 80,000 `bt-token-v1` or is split at a valid boundary.
- The baseline-qualified catalog and applicability ledger contain exactly one `PASS`/`FAIL`/`BLOCKED`/`UNVERIFIED` row per applicable item; all `FAIL`/`UNVERIFIED` rows become candidates and all `BLOCKED` rows receive a rerun or an explicit unresolved result.
- Every shard has one fresh Luna report and the consolidator's Temp index proves exact coverage with no missing, duplicate, or baseline-mismatched row; every row and candidate has decisive evidence or an explicit unresolved reason.
- Every candidate has one Sol disposition; every retained or promoted serious candidate has a disproof attempt against source, authorities, callers, guards, and lifecycle, plus one locator verdict for each atomic external claim.
- Refuted/advisory candidates have no tracked record; every serious survivor has exactly one Plan or Investigation route with decision-complete evidence or an exact duplicate mapping, and every resulting Plan validates. Frozen hashes remain unchanged and no source/GLSL/script/skill/instruction diff is introduced.

## Notes

This Plan has no dependency and is independently executable. The current evidence is a preparation signal, not a substitute for the execution freeze. The Plan creation stage itself requires the landing gate; future audit output follows the normal Tier-3 review, verification, and landing workflow.
