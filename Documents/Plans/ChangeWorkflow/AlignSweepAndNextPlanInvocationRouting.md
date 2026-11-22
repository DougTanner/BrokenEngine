<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T23:53:00.582Z","dependsOn":[]} -->
# Align `/sweep` and `/next-plan` invocation and host-routing contracts

## Context

Validation of `.agents/skills/sweep` under `/external-skill-creator` validate
mode surfaced two pre-existing Critical cross-skill invocation and routing
conflicts between `/sweep` and `/next-plan` (both present before and outside
the `codex-bridge` relocation and invocation-control alignment change):

1. **Claude-only `/sweep` boundary vs. unconditional `/next-plan` Sweep
   routing**:
   `.agents/skills/sweep/SKILL.md:3-11,33-34` restricts `/sweep` to Claude Code
   sessions ("Claude Code only: the coordinator launches Codex, so Codex
   callers never invoke it", with `Invoke-Sweep.ps1:99,212` launching `codex`
   directly in a parallel loop and `SKILL.md:12,105` relying on Claude Code
   tools such as `Monitor` and `AskUserQuestion`). Meanwhile,
   `.agents/skills/next-plan/SKILL.md:3,18-22` accepts explicit Codex
   (`$next-plan`) and Antigravity CLI (`/next-plan`) invocation,
   `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:174-191` claims
   any matching Plan without checking host or `## Sweep`, and
   `.agents/skills/next-plan/SKILL.md:390-395` unconditionally directs main to
   run `/sweep` whenever the claimed Plan carries a `## Sweep` section. A
   non-Claude host running `/next-plan` or `$next-plan` on a Sweep Plan can
   therefore claim a Plan it cannot execute and is instructed to invoke a
   Claude-only skill.
2. **No-Plan `/sweep` flow instructing the agent to invoke `/next-plan`**:
   `.agents/skills/sweep/SKILL.md:27-31` tells the agent, when the user asks
   for a sweep without a saved Plan, to "ask the user to save one with
   `/save-plan` ... then run `/next-plan <Plan>`. `/save-plan` is
   user-invoked, so never run it yourself." That phrasing tells the agent to
   invoke `/next-plan <Plan>` itself, which contradicts
   `.agents/skills/next-plan/SKILL.md:3-4,18-22` (`disable-model-invocation:
   true`; invoke only when the latest user request explicitly asks to execute
   `/next-plan` or `$next-plan` and claim a Plan).

## Design

1. **Gate Sweep Plans to Claude Code before claiming in
   `Invoke-NextPlanClaim.ps1` (author recommendation)**:
   - Rationale: Root `AGENTS.md` directs encoding a rule in a script whenever a
     program can decide it, leaving prose for judgment. `Get-NextPlanContext`
     (`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:21,44`)
     already exposes `$context.SessionBranch` (`claude/<uuid>` vs.
     `codex/<uuid>` or `agy/<uuid>`). Enforcing the guard inside
     `Invoke-NextPlanClaim.ps1` closes both the bare-claim TOCTOU race
     (`Tools/WorktreeCli/PlanScheduler.cpp:888` skips a peer-claimed head
     candidate on untargeted `plan claim-next`) and partial-name suppression of
     `claim.plan-mismatch` (`Tools/WorktreeCli/PlanScheduler.cpp:848-854`),
     without modifying `Tools/WorktreeCli/` C++ code.
   - Sweep detection and refusal contract:
     - Trigger whenever `$context.SessionBranch` does not start with `claude/`
       (case-sensitive prefix match).
     - Inspect a candidate's committed bytes at the session commit used for
       selection (for both main repo `HEAD` and session Coretide clone `HEAD`,
       matching `Tools/WorktreeCli/PlanScheduler.cpp:664,838-839`, rather than
       the working tree, failing closed on read error) for `## Sweep` (using
       `Invoke-Sweep.ps1:53`'s heading pattern `(?ms)^## Sweep[ \t]*\r?\n`)
       only after resolving its exact case-sensitive ordinal scheduler path
       identity (matching `Tools/WorktreeCli/PlanScheduler.cpp:869`, e.g.,
       against `plan list`, failing closed on listing error; paths returned by
       `plan claim-status` and `plan list` are already scheduler-canonical). An
       exact `-Plan` path with a wrong-case filename stem and lowercase `.md`
       extension has no matching scheduler candidate and passes through to
       `plan claim-next` (`none` / `none-available`) rather than returning a
       Sweep refusal; invalid paths such as uppercase `.MD` stop earlier at the
       authoritative `plan claim-status` gate (`invalid-plan` per
       `Tools/WorktreeCli/PlanScheduler.cpp:947-950` and
       `Tools/WorktreeCli/PlanMetadata.cpp:43-54`).
     - Stop a refused Sweep Plan via `Complete-Claim` with exit `2`,
       `status: 'blocked'`, `code: 'claim.sweep-requires-claude'`,
       `nextAction: 'stop-report-to-user'`, and a message naming the Plan and
       directing the user to run `/next-plan <Plan>` in a Claude Code session.
       Populate `$result.claim` on that stop only when this session still holds
       a live claim (so `Complete-Claim` appends the held-claim notice when
       live, and the message states that no Plan was claimed otherwise).
   - Required behavioral invariants on non-`claude/` sessions:
     - **Early at-tip reuse (`Invoke-NextPlanClaim.ps1:80-83`)**:
       `Get-NextPlanClaimStatus` already reports `claim.plan-mismatch` first
       for exact `-Plan` mismatches. When an at-tip session holds a live claim
       whose canonical Plan carries `## Sweep`, stop with
       `claim.sweep-requires-claude` (keeping `$result.claim` populated)
       instead of returning `reused`.
     - **Pre-validation target pinning (`Invoke-NextPlanClaim.ps1:99-126,
       189-191`)**: Preserve any target pinned by the early bare held-claim
       lookup independently of post-validation claim status. Because lines
       `99-126` already permitted retained dirty work based on that claim,
       ordinary scheduler healing of an expired, orphaned, or primary-absent
       claim during `plan validate`
       (`Tools/WorktreeCli/PlanScheduler.cpp:246-267,573-583`) clears only
       live held-claim status; the request stays pinned to that original Plan
       via `--plan` so it keeps targeted `none-available` or `claim.plan-moved`
       rename handling and never drops into the bare queue over retained work.
     - **Authoritative post-validate held-claim precedence
       (`Invoke-NextPlanClaim.ps1:174-191`)**: Because the early lookup skips
       partial `-Plan` patterns (`:81`) and falls through on `busy` or errors,
       query `Get-NextPlanClaimStatus` authoritatively after `plan validate`,
       integrity checks, and partial-pattern resolution (passing the exact or
       resolved `-Plan` or pinned target when present) before any `## Sweep`
       check or `plan claim-next`:
       - On `claim-plan-mismatch` (exit `2`), populate `$result.conflict`
         (`requestedPlan`, `heldPlan`) and stop with `claim.plan-mismatch`
         (`exit 2`, `status: 'blocked'`, `nextAction: 'stop-report-to-user'`),
         preserving mismatch precedence for resolved partial names and
         early-lookup fallthroughs before any Sweep refusal runs.
       - On `busy`, `guard-unavailable`, any other non-zero exit, or malformed
         JSON, fail closed (`scheduler.busy`, `scheduler.guard-unavailable`, or
         `claim.rejected` / `claim.failed`) instead of falling through to a
         Sweep refusal that could hide a held claim or mismatch.
       - Update live held-claim status from this result (`claimed` vs. `none`)
         without clearing a pre-validation pinned target, leaving normal
         `plan claim-next` to own `$result.claim` whenever execution proceeds
         past the Sweep guard.
     - **Targeted or pinned path**: Whenever `-Plan`, a pre-validation pinned
       Plan, or a live held claim identifies a single target, verify its exact
       case-sensitive ordinal scheduler path identity before calling
       `plan claim-next`. If it matches a scheduler candidate carrying
       `## Sweep`, stop with `claim.sweep-requires-claude` (also preventing
       `-UserAuthorizedTakeover` from releasing a live Sweep claim held by
       another session); otherwise proceed to `plan claim-next --plan <target>`
       with existing `none-available`, `claim.plan-moved`, and takeover
       handling.
     - **Bare path with no target, pinned Plan, or live held claim**: Never
       invoke untargeted `plan claim-next` on a non-`claude/` session. Track
       attempted normalized paths in a case-sensitive set and iterate:
       1. Query `WorktreeCli plan list --repo ... --worktree ...` (same
          `(createdUtc descending, normalized path)` order and primary/session
          eligibility rules as `plan claim-next`, per
          `Tools/WorktreeCli/PlanScheduler.cpp:681-749,884-887`), failing
          closed on non-zero exit or malformed JSON.
       2. Pick the first `eligible` row not yet attempted; if none exists, stop
          with `none-available` (`exit 0`, `status: 'pass'`,
          `message: 'No eligible Plan is available.'`,
          `nextAction: 'checkpoint-followup-gate'`).
       3. If that candidate carries `## Sweep`, stop with
          `claim.sweep-requires-claude` without calling `plan claim-next`.
       4. Otherwise record the candidate as attempted and invoke
          `plan claim-next --plan <candidate>` so `PlanScheduler.cpp:869` can
          claim only that checked candidate and can never skip to an unchecked
          Sweep Plan. Handle `claim-plan-mismatch` and non-zero exits as today;
          if `plan claim-next` returns `none` (concurrent claim, completion,
          blocking, or an unhealable record), loop to re-query `plan list`
          instead of calling `Get-TargetedNoneMessage`.
   - Document `claim.sweep-requires-claude`, held-claim / `claim.plan-mismatch`
     precedence for exact and resolved partial names, pre-validation target
     pinning, and the non-`claude/` bare candidate-binding behavior in
     `.agents/skills/next-plan/references/claim-results.md`.
2. **Gate the `## Sweep` routing rule in `/next-plan` to Claude Code**:
   - In `.agents/skills/next-plan/SKILL.md:390-395` (`## Rules`), gate the
     `## Sweep` route explicitly to Claude Code sessions. State that non-Claude
     hosts (Codex, Antigravity CLI) never invoke `/sweep`:
     `Invoke-NextPlanClaim.ps1` stops before claiming a Sweep Plan and directs
     the user to Claude Code, and if a non-Claude host ever holds a pre-existing
     claim on a Sweep Plan, it stops as a blocker with the claim held
     (`### Post-checkpoint outcomes`) and directs the user to Claude Code
     without invoking `/sweep`.
3. **Align `/sweep` `## When to use` and `description` with both contracts**:
   - In `.agents/skills/sweep/SKILL.md:27-31`, reword the no-Plan workflow so
     the agent asks the user to save the Plan with `/save-plan` and then asks
     the user to invoke `/next-plan <Plan>` themselves, stating that both
     `/save-plan` and `/next-plan` are user-invoked so the agent never invokes
     either.
   - In `.agents/skills/sweep/SKILL.md:3-11,25-26,33-34`, state consistently
     that `/sweep` is Claude Code only (neither Codex nor Antigravity CLI
     callers invoke it, and non-Claude sessions direct the user to a Claude
     Code session before claiming or running a Sweep Plan).

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (the `/next-plan` claim guard in `Invoke-NextPlanClaim.ps1` and the
`/next-plan` / `/sweep` skill routing contract). No determinism/CRC, wire,
serialization, `.pack`/`kiVersion`, replay, affinity, threading, allocation,
shader, build, trust-boundary, or live `/agent-harness` exposure.

## Critical files

- `.agents/skills/sweep/SKILL.md` — frontmatter `description` (`:3-11`) and
  `## When to use` (`:23-35`).
- `.agents/skills/next-plan/SKILL.md` — `## Rules` `## Sweep` routing bullet
  (`:390-395`).
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` — early at-tip
  held-claim reuse (`:80-83`), pre-validation target pinning
  (`:99-126,189-191`), pattern resolution, post-validate held-claim precedence
  check, case-sensitive `## Sweep` guard, and bound bare/targeted
  `plan claim-next` loop (`:174-210`).
- `.agents/skills/next-plan/references/claim-results.md` — early reuse /
  mismatch rules (`:20-33`) and pre-claim `stop-report-to-user` result
  documentation (`:133-160`).

## In scope

- `.agents/skills/sweep/SKILL.md`: `description` (`:3-11`) and `## When to use`
  (`:23-35`) to state the Claude Code-only boundary for all non-Claude hosts
  and to direct the user (never the agent) to invoke `/next-plan <Plan>` after
  `/save-plan`.
- `.agents/skills/next-plan/SKILL.md`: `## Rules` (`:390-395`) to gate the
  `## Sweep` route to Claude Code and specify the non-Claude stop/redirect
  behavior.
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`: non-`claude/`
  `## Sweep` check on the early at-tip held-claim path (`:80-83`); preservation
  of any pre-validation pinned bare target across `plan validate` healing
  (`:99-126,189-191`); post-validate authoritative `Get-NextPlanClaimStatus`
  precedence check after pattern resolution (`:174-191`); case-sensitive
  ordinal scheduler path resolution before targeted/pinned `## Sweep` refusal
  and `plan claim-next`; and bare candidate-binding loop via `plan list` plus
  explicit `plan claim-next --plan <candidate>` (`:191-210`), returning
  `claim.sweep-requires-claude` (`exit 2`, `status: blocked`,
  `nextAction: stop-report-to-user`).
- `.agents/skills/next-plan/references/claim-results.md`: document
  `claim.sweep-requires-claude`, held-claim / `claim.plan-mismatch` precedence
  on resolved partial names, pre-validation target pinning, and the bare
  non-`claude/` bound-candidate flow.

## Out of scope

- `Tools/WorktreeCli/` C++ scheduler code or `plan claim-next` selection
  semantics (no C++ build or WorktreeCli change).
- `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` coordinator mechanics or
  `.agents/skills/sweep/agents/openai.yaml` (already sets
  `policy.allow_implicit_invocation: false`; Codex project-level disablement is
  unsupported per `client-compatibility.md:86-94`).
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1` or OpenCode
  branch cleanup.
- Any C++, GLSL, or Visual Studio project file.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/sweep`
  and `-Path .agents/skills/next-plan` both exit `0` with `VALID`.
- `/external-skill-creator` validate-mode semantic invocation review on
  `.agents/skills/sweep` and `.agents/skills/next-plan` reports zero conflicts
  between `/sweep`'s Claude-only boundary, `/next-plan`'s multi-host explicit
  invocation and `## Sweep` routing, and `/sweep`'s no-Plan user-invocation
  handoff to `/save-plan` and `/next-plan`.
- Direct verification of `Invoke-NextPlanClaim.ps1` on a non-`claude/` session
  confirms all paths without acquiring a Sweep claim or, on a request targeting
  a Sweep Plan, releasing a live Sweep claim for takeover (the pre-existing
  owner/session `unclaim` race on non-Sweep takeover requests at
  `Invoke-NextPlanClaim.ps1:201-207` / `PlanScheduler.cpp:1001-1016` is out of
  scope; ordinary scheduler healing of expired, orphaned, or primary-absent
  claims during `plan validate` per
  `Tools/WorktreeCli/PlanScheduler.cpp:246-267,573-583` still applies):
  - Targeted `-Plan` (exact or partial) on a `## Sweep` Plan with no held claim
    exits `2` with `code: claim.sweep-requires-claude` and
    `nextAction: stop-report-to-user` without claiming the Plan or releasing a
    peer's live Sweep claim under `-UserAuthorizedTakeover`, while a normal
    non-Sweep Plan claim path is unaffected, an exact `-Plan` path with a
    wrong-case filename stem and lowercase `.md` extension passes through to
    the scheduler's `none` (`none-available`) result rather than returning
    `claim.sweep-requires-claude`, and existing invalid-path handling (e.g.,
    `.MD` -> `invalid-plan` per `Tools/WorktreeCli/PlanMetadata.cpp:43-54`)
    stays preserved.
  - Bare run with no held claim when the top eligible Plan is a `## Sweep` Plan
    exits `2` with `code: claim.sweep-requires-claude` without claiming any
    Plan.
  - Bare run contention path: when the top eligible non-Sweep candidate becomes
    unavailable between `plan list` and `plan claim-next` (simulated by a peer
    claim or `none` return) and the next eligible candidate is a `## Sweep`
    Plan, the script targets candidates explicitly with `--plan` (never
    invoking untargeted `plan claim-next`), re-queries `plan list`, and stops
    with `claim.sweep-requires-claude` without acquiring the Sweep Plan (and
    claims the next non-Sweep candidate or returns `none-available` when no
    Sweep Plan blocks it).
  - Off-tip bare held-claim healing: when an off-tip bare run held a claim on
    Plan `A` before `plan validate` and validation heals that claim away (e.g.,
    absent or renamed at primary tip), the script stays pinned to `A` via
    `--plan` and preserves targeted `none-available` or rename
    (`claim.plan-moved`) handling even when another Plan is eligible, never
    dropping into the bare queue to claim another Plan.
  - Partial-name held-claim precedence: when the session already holds a claim
    on Plan `A` and invokes `Invoke-NextPlanClaim.ps1` with a partial `-Plan`
    pattern that resolves to a different `## Sweep` Plan `B`, the script exits
    `2` with `code: claim.plan-mismatch` and `conflict` naming `heldPlan: A`
    and `requestedPlan: B` rather than suppressing the mismatch with
    `claim.sweep-requires-claude`.
  - Retained-edit committed-bytes check: when a held-claim path (main repo or
    Coretide clone) carries a retained working-tree edit that removes `## Sweep`
    from a committed Sweep Plan, the guard inspects the Plan's committed bytes
    at the session commit used for selection and still stops with
    `claim.sweep-requires-claude` (keeping `$result.claim` populated when the
    claim is live) instead of reusing or claiming the Plan.
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -IncludeUntracked`
  passes with zero failures.

## Notes

- `.agents/skills/sweep/agents/openai.yaml:1-2` already sets
  `policy.allow_implicit_invocation: false`. As verified on 2026-11-21 in
  `codex-cli 0.170.0`
  (`.agents/skills/external-skill-creator/references/client-compatibility.md:86-94`),
  project `.codex/config.toml` does not disable explicit `$sweep` invocation,
  so gating in `/next-plan`, `Invoke-NextPlanClaim.ps1`, and `/sweep` prose is
  the repo-enforceable boundary.
- Read-only scheduler contract verified in `Tools/WorktreeCli/PlanScheduler.cpp`:
  `RunValidate` (`:573-585`) calls `HealClaims` (`:246-267`), which deletes
  expired, orphaned, or primary-absent claims before claim selection; `RunList`
  (`:681-749`) uses the same `(createdUtc descending, normalized path)`
  ordering and primary/session eligibility checks as `RunClaimNext`
  (`:859-887`); `RunClaimStatus` (`:952-969`) and `RunClaimNext` (`:848-856`)
  both enforce held-claim / `claim-plan-mismatch` precedence under
  `scheduler.guard`; and `RunClaimNext` (`:869-872`) matches `requestedPlan`
  case-sensitively whenever `--plan` is supplied.
- Live `/agent-harness` verification does not apply; verification uses skill
  validation, static semantic review, claim-script checks, and
  `Invoke-StaticChecks.ps1`.
