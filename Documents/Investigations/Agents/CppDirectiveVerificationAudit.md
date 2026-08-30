# C++ Directive Verification Audit

## Status

Ready for explicit manual execution in a new session. This is a non-scheduler Investigation, not an executable Plan, and this session does not execute the scan.

## Context

This is a one-time migration scan for clear directive and invariant violations in the first-party C++ corpus. The current audit execution is deferred: this session stopped before scanning source because its evidence machinery became overengineered. Existing ignored output under `Temp/CppDirectiveVerificationAudit/` is not input, evidence, or completion.

A future session reviews the live current worktree. Earlier preparation reported 529 first-party C++ targets and four dual-language headers; those numbers are historical preparation evidence only, not the target count for a future run.

## Simple Design

1. At the start of the fresh session, capture starting `git status` and existing owned paths. The audit session has exclusive read-only ownership of all target and authority files until candidate routing; no agent or concurrent task writes them. Enumerate current first-party C++ paths from the live worktree, including tracked and non-ignored untracked paths, applying the exact classifier path rules in `.agents/scripts/Get-SessionChangeInventory.ps1` under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/` while excluding `ThirdParty/` and generated paths. Write one small manifest under `Temp/CppDirectiveVerificationAudit/<session-id>/` containing each target path, class, exact authority chain, and `bt-token-v1` size. Read the applicable `AGENTS.md`, architecture sources, and `.agents/skills/repo-code-review/SKILL.md` from the live worktree. Do not create custom attempt, version, or review ledgers or an audit database.

2. Deterministically group targets by subsystem, exact authority chain, and path. Keep each shard at or below 80,000 `bt-token-v1`, including its assigned source and authority context. Use one fresh Luna `researcher` per shard, with fork none and no children, in waves within host concurrency. Assign every target exactly once. Researchers read live bytes. Evidence may cross shard, but the candidate anchor remains owned by its target shard.

3. Each shard owns one ignored Markdown report under `Temp/CppDirectiveVerificationAudit/<session-id>/<shard>.md`. The report contains session metadata, starting status and owned paths, the exact file list, authorities read, and a per-file `reviewed/no candidate` entry or candidate IDs. Each candidate records severity, confidence, occurrence, path:line and symbol, controlling directive or concept, concrete claim, evidence, reachability or refutation, impact, next check, and any external claim. Use `observed` for directly seen behavior or evidence, `credible_exposure` for a concrete reachable risk not observed to fail, and `hypothetical` for a merely possible or speculative path. `LOW` is weak or cosmetic, `MEDIUM` localized, `HIGH` major and credible or observed, and `CRITICAL` catastrophic or core; hypothetical candidates cannot exceed MEDIUM. Do not build a global atomic directive catalog or require a PASS row for every sentence. `BLOCKED` is transient: if required evidence cannot run, the audit stage blocks; an attempted check that runs but is inconclusive becomes an `UNVERIFIED` candidate.

4. Before consolidation, the audit session reruns the same live tracked and non-ignored untracked enumeration and `git status` snapshot, compares paths and status with the starting snapshots, incorporates additions, deletions, and renames into deterministic affected shards, and reruns those shards. Any reported or detected content change in a target or authority file reruns its affected shard before consolidation. The exclusive read-only ownership contract is the content-stability guarantee. One Luna `implementer` owns consolidation and index creation: it mechanically verifies the manifest, path-once coverage, and report coverage, then creates one Markdown index without semantic rewriting. A fresh Sol `reviewer` gives every candidate one disposition in bounded groups; LOW and MEDIUM advisories default to drop but may be promoted. Sol re-reads each candidate against the current bytes of the source, authorities, callers, guards, and lifecycle and tries to disprove every retained or promoted HIGH or CRITICAL. Collect all atomic external claims from all candidate reviews and dispatch exactly one Luna `locator` through `/verify-external-claims` for the complete claim set. Sol reviewer dispositions remain provisional until the locator verdicts return; resume the owning Sol reviewers with those verdicts, and make final disposition, deep verdict, and routing incorporate them. An `UNRESOLVED` locator verdict remains unresolved/`UNVERIFIED`, not a confirmed serious route. Use compile or harness checks only when directly decisive; do not instrument.

5. Refuted and final LOW or MEDIUM advisories stay Temp-only. Serious means final HIGH or CRITICAL. Route decision-complete serious survivors through `/create-follow-up-plans`; route serious cases with open intended-behavior, cause, correction, or check decisions to the owning Investigation. Search for exact duplicates first. No source fixes or capability work are authorized.

## Critical files

- `AGENTS.md`, `Documents/AGENTS.md`, `Documents/Investigations/AGENTS.md`, and every applicable nested `AGENTS.md`
- `.agents/scripts/Get-SessionChangeInventory.ps1` and `.agents/skills/repo-code-review/SKILL.md`
- Applicable architecture authorities: `Documents/Architecture/FrameUpdatePipeline.md`, `Documents/Architecture/GameReconciliation.md`, and `Documents/Architecture/Network.md`
- First-party C++ targets and their callers, consumers, producers, mirrors, and authority documents under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`

## In scope

- A live-worktree manifest containing tracked and non-ignored untracked target path, class, authority chain, and token size, plus starting Git status and owned paths.
- Every first-party C++ target selected by the live classifier exactly once for this lens, including dual-language headers.
- Reading each target's authority chain, bounded shard reports, one mechanical coverage index, candidate triage, Sol disproof, external-claim checks, and serious-survivor routing.

## Out of scope

- Directive-to-implementation, current-consumer or mirror traceability, hidden defaults, contradictions, and orphan surfaces (CppPlanTraceAudit).
- KISS, worth or cost, simpler mechanisms, root-cause versus bandaid, actual-consumer benefit, and representation fit (CppPlanSimplicityAudit).
- Current documented subsystem, layer, client/server, CPU/GPU ownership or placement boundaries (CppScopeBoundaryAudit).
- Standalone reachable runtime or integration falsification (CppAdversarialInvariantAudit).
- Comment or documentation fact placement and wording or style (CppProgressiveDisclosureAudit).
- The dedicated four-header CPU/GLSL layout and binding audit (CppSharedHeaderGlslAudit).
- An exhaustive global directive catalog, atomic sentence ledger, custom schemas, or internal attempt, version, or review ledgers; source fixes, instrumentation, capability additions, unit tests, style cleanup, metrics, generic repo review, generated output outside ignored `Temp/`, and any scan in the current session.

## Risk tier and invariants

Authoring this replacement is Tier 1 documentation. Executing the described scan is Tier 3 because its corpus spans independently owned subsystems and serious survivors may need durable routing. Coverage is path-based and starts from the live-worktree manifest. The audit session has exclusive read-only ownership of target and authority files until candidate routing; no agent or concurrent task writes them, and this ownership is the content-stability contract. Before consolidation, rerun the same live tracked and non-ignored untracked enumeration and `git status` snapshot, compare paths and status with the starting snapshots, incorporate additions, deletions, and renames into deterministic affected shards, and rerun them. Any reported or detected target or authority content change reruns its affected shard. Every target is assigned once, every assigned file gets a report completion entry, every candidate gets one final Sol disposition after applicable locator verdicts return, every serious survivor gets a deep verdict and one route, and an `UNRESOLVED` external claim remains unresolved/`UNVERIFIED`, not a confirmed serious route. This Investigation remains non-scheduler material; reports and the index are ignored Temp output only.

## Acceptance criteria

- The fresh dynamic manifest records its live tracked and non-ignored untracked target count and class set, captures starting Git status and owned paths, and excludes non-first-party or generated paths. The earlier 529-target and four-dual-language-header count remains historical preparation evidence only.
- The audit session has exclusive read-only ownership of target and authority files until candidate routing, with no agent or concurrent task writing them. Before consolidation, it reruns the same live tracked and non-ignored untracked enumeration and Git-status snapshot, compares paths and status with the starting snapshots, incorporates additions, deletions, and renames into deterministic affected shards, and reruns those shards; any reported or detected target or authority content change reruns its affected shard.
- Coverage proves every manifest target appears once, every assigned authority chain is read, every shard is at most 80,000 `bt-token-v1` including source and authority context, and each report and index covers its assigned paths.
- Every assigned file has a `reviewed/no candidate` entry or candidate IDs, and every candidate has the required evidence fields and severity or occurrence classification. Acceptance does not require a machine row for every directive sentence.
- All atomic external claims collected from all candidate reviews go through exactly one Luna `locator` via `/verify-external-claims`. Sol reviewer dispositions remain provisional until locator verdicts return; the owning Sol reviewers resume with those verdicts, and final disposition, deep verdict, and routing incorporate them. Every candidate has one final Sol disposition; Sol re-reads each candidate against current bytes, and every final HIGH or CRITICAL has a bounded disproof attempt against source, authorities, callers, guards, and lifecycle. An `UNRESOLVED` locator verdict remains unresolved/`UNVERIFIED`, not a confirmed serious route.
- Serious decision-complete survivors route through the appropriate follow-up Plan, open decisions route to the owning Investigation, and exact duplicates are mapped. Refuted and LOW or MEDIUM advisories remain Temp-only.
- Any target or authority change during the scan reruns only its affected shard before consolidation, with no global restart; no tracked audit advisory or custom audit database is produced.

## Notes

This runbook is for a new session only. The old aborted output in `Temp/CppDirectiveVerificationAudit/` is ignored and is not a dependency, claim, or future input. The user explicitly chose a simple human-review swarm with a small mechanical coverage index over an audit database.
