<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-01T23:33:34.214Z","dependsOn":[]} -->
# Investigate and fix: Codex implementer/researcher children observed running on the reviewer model instead of the mapped model

## Context

The `/next-plan-review` of the Codex landing `c32b72b09983a2618bf3ef4db3433753e548a0fa`
(Codex parent session `01a05d65-7d54-7b32-831c-6cc02eccb9da`, worktree/branch UUID
`4812b9b9-8eba-4096-a2d0-efa42e3127a9`) found that the observed session's delegated
children did not all run on the model the root `AGENTS.md` role table pins for their role.

Evidence, taken from the parent's own `SubAgentActivity` inventory (47 direct children,
all depth 1) and each child's host-written `turn_context`, which records the model and
reasoning effort the child actually ran with:

- Planning child (1): `gpt-5.6-sol` / `max` — matches the mapping (planner -> Fable -> sol max).
- Review/audit children (20): `gpt-5.6-sol` / `medium` — matches (reviewer -> Sol -> sol medium).
- Build/mechanical children (10): `gpt-5.6-luna` / `max` — matches (builder and mechanic -> Sonnet -> luna max).
- Implementation, propagation, docs, and finalization children (11): `gpt-5.6-sol` / `medium` —
  does not match; the mapping requires implementer -> Opus -> `gpt-5.6-luna` / `max`.
- Research/diagnosis children (5): `gpt-5.6-sol` / `medium` — does not match for
  researcher -> Opus -> `gpt-5.6-luna` / `max`. One of these five (a runtime-verification
  child) is arguably a reviewer concern, so it is counted as unverified.

That is 15 confirmed off-mapping children (16 including the unverified one), covering
92.9 of 303.3 measured child-minutes (30.6% of delegated work). The requested role or
model per dispatch is not recoverable: the `spawn_agent` message payloads are stored
encrypted, so only the resulting `turn_context` is observable.

Observed pattern: every implementation and research concern landed on `sol` / `medium`,
which is also the parent session's model and effort, while every build and mechanical
concern correctly landed on `luna` / `max`. Because the requested agent name is
unrecoverable, this evidence does not distinguish a one-session dispatch error from a
mechanism-level defect.

Root cause is narrowed but not yet proven. The four `.codex/agents/*.toml` definitions do
carry correct pins today — `opus.toml` is `model = "gpt-5.6-luna"` with
`model_reasoning_effort = "max"` — so the remaining candidates are (a) the parent naming
the wrong agent (`sol`) when spawning implementer and researcher work, (b) the agent name
failing to resolve so the host silently inherits the session model, or (c) repository
routing prose that does not tell a Codex parent which `.codex/agents` name each role
resolves to. Root `AGENTS.md` and its line "ChatGPT Codex: Fable -> gpt-5.6-sol max;
Sol -> gpt-5.6-sol medium; Opus and Sonnet -> gpt-5.6-luna max" is the governing contract.

## Design

1. Spawn one `opus`-named Codex child explicitly and read its `turn_context` model and
   effort. Confirm from the live `.codex/agents/*.toml` files whether the `opus` name
   resolves.
2. If explicit `opus` records `luna` / `max`, change routing prose only if the current
   instructions contain a concrete ambiguity that directs implementer or researcher work
   to another agent name; otherwise report that the historical cause cannot be proven and
   make no repository fix. If explicit `opus` records `sol` / `medium`, fix the proven
   name-resolution or pin failure in `.codex/agents/opus.toml` or the `.codex`
   configuration that registers it. Do not do both, and do not add a new validation
   mechanism, script, or check for a failure mode step 1 did not observe.

## Critical files

- `AGENTS.md` — the role table and the ChatGPT Codex mapping line (routing contract)
- `.codex/agents/opus.toml` — the pinned model and effort for Opus-mapped roles
- `.codex/agents/sol.toml`, `.codex/agents/fable.toml`, `.codex/agents/sonnet.toml` — the
  sibling definitions the working roles resolve through, for comparison only
- `.codex/codex-review.ps1` — the one repository dispatch path that names a model, for
  comparison with the in-session `spawn_agent` route

## In scope

- Observing what model and effort an explicitly `opus`-named Codex child records, and
  proving a name-resolution or pin failure if that observation shows one
- The smallest resulting fix, confined to the files named under `## Critical files`,
  limited to the role-to-agent-name routing statements and the `opus` agent's model and
  effort pin, or no fix at all when explicit opus resolves correctly and the current instructions
  contain no concrete routing ambiguity

## Out of scope

- The landed change `c32b72b0` itself and anything else that Codex session produced
- Claude Code delegation, `subagent_type` routing, and the `/codex-review` reviewer route,
  none of which the evidence shows misrouting
- Reviewer, planner, builder, and mechanic role mappings, all of which matched
- Any new script, validator, telemetry, or enforcement mechanism for dispatch model checks
- Any transcript path, transcript text, or home-directory path in the repository

## Risk tier and invariants

Expected Tier 1 when the fix is routing prose only (documentation with no behavior
surface), and Tier 2 when it changes `.codex/agents/opus.toml` or the `.codex`
configuration, because that is a delegation and routing rule change to one tool's
behavior. Highest applicable trigger governs; escalate if the investigation shows the fix
must reach the shared agent-dispatch configuration used by other sessions, which is
build/bootstrap-style coordination. Invariant preserved: the root `AGENTS.md` role table
remains the single authoritative routing policy, with `.codex/agents/` resolving roles
through its Model column, and no fact about the mapping is duplicated into a second
owning location.

## Acceptance criteria

- An explicitly `opus`-named Codex child produces a `turn_context` recording
  `gpt-5.6-luna` and `max`, either already at step 1 or after the step 2 fix
- When step 1 already records `gpt-5.6-luna` and `max` and no concrete routing ambiguity
  exists, the recorded finding that the historical cause cannot be proven closes the Plan
  with no repository change
- Reviewer, planner, builder, and mechanic children still record their previously
  matching models and efforts
- `/validate-skill` passes for any changed `SKILL.md`; plan validate exits 0

## Notes

- The 30.6% of delegated child-minutes figure measures exposure, not correctness of the
  landed change; the observed session's output was accepted separately.
- Retroactive proof of what each dispatch requested is impossible because `spawn_agent`
  payloads are stored encrypted, so verification must come from a fresh reproduction.
