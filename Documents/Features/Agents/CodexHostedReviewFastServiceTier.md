# Run Codex-hosted Sol reviews on the fast service tier

Revisit When: Codex-hosted Sol review turnaround becomes a bottleneck worth the roughly 2.5x fast-tier credit burn for the reviewer role.

## Context

Reviews dispatched from Claude Code already run on Codex's fast (priority) service tier: `.codex/codex-review.ps1:225` passes `-c 'service_tier="fast"'` to the detached `codex exec` invocation. That change was deliberately scoped to the Claude Code route only.

Codex-hosted sessions never call that script. Their `reviewer` role resolves to `.codex/agents/sol.toml`, which sets only `model = "gpt-5.6-sol"` and `model_reasoning_effort = "medium"` and carries no service-tier setting, so the tier falls back to the user's `~/.codex/config.toml`. The suggested maintainer profile documented at `README.md:151` sets `service_tier = "default"`. Root cause: Sol reviews started inside Codex therefore run on the standard tier while the identical review started from Claude Code runs fast, purely because of where it was dispatched.

External behavior verified against the openai/codex sources during the session that recorded this Plan: `service_tier` is a documented top-level Codex config key accepting `fast` and `flex` (wire value `priority`); the `fast_mode` feature flag is Stable and default-enabled; gpt-5.6 models advertise the fast tier; a tier the model does not advertise degrades silently to standard with a warning. Cost under ChatGPT sign-in is roughly 2.5x subscription credit burn for roughly 1.5x speed on GPT-5.6, which is why fast tier is wanted for the medium-effort Sol reviewer and explicitly not wanted for the Luna workers.

One fact is unverified and must be settled first: whether a per-agent TOML under `.codex/agents/` honors a `service_tier` key at all. Only the top-level `~/.codex/config.toml` key is confirmed.

## Design

Step 1 — settle the open fact. Determine from the installed Codex CLI's own agent-config handling whether a per-agent TOML under `.codex/agents/` accepts and applies `service_tier`. Acceptable evidence is the Codex CLI's documented agent-config key set plus an observed run: add the key to `.codex/agents/sol.toml`, dispatch one trivial Sol review from a Codex-hosted session, and confirm the run is accepted without an unknown-key or unadvertised-tier warning.

Step 2 — branch on that result; both outcomes are decided here, so no further approval round is needed:

- Supported: keep `service_tier = "fast"` in `.codex/agents/sol.toml` only. Do not add it to `fable.toml`, `opus.toml`, or `sonnet.toml` — the 2.5x credit burn is accepted for the reviewer alone.
- Not supported: revert `.codex/agents/sol.toml` to its current bytes and instead record the limitation in `README.md` next to the maintainer profile at `README.md:151`: Codex-hosted reviews run on whatever tier `~/.codex/config.toml` sets, and a maintainer who wants them fast sets that key machine-wide at the cost of every Codex worker burning fast-tier credit. A machine-wide `service_tier = "fast"` is rejected as this Plan's own change because it would also apply to the Luna worker roles.

Nothing else changes. `.codex/codex-review.ps1` already handles the Claude Code route correctly and is out of scope.

## Critical files

- `.codex/agents/sol.toml` — the reviewer role definition that would carry the key
- `README.md` — maintainer `~/.codex/config.toml` profile at line 151, the documentation fallback
- `.codex/codex-review.ps1` — reference only for the already-working Claude Code route; not to be changed

## In scope

- Verifying whether `.codex/agents/*.toml` honors a `service_tier` key
- Adding `service_tier = "fast"` to `.codex/agents/sol.toml` when supported
- Adding the limitation note beside the maintainer config block in `README.md` when it is not supported

## Out of scope

- `.codex/codex-review.ps1` and the Claude Code review route
- `.codex/agents/fable.toml`, `opus.toml`, `sonnet.toml`, and any non-reviewer role
- Changing any user's `~/.codex/config.toml`, which is machine-local and untracked
- Reasoning-effort, model selection, or any other role attribute

## Risk tier and invariants

Expected Change Workflow Tier 2 — scoped tool behavior for the Codex review dispatch route. Trigger: it changes the configuration surface that governs how delegated reviews execute. No engine code, determinism/CRC, serialization, wire, or build coordination surface is touched. The role table in the root `AGENTS.md` must keep mapping `reviewer` to Sol at medium effort; this Plan changes only the service tier, never model or effort.

## Acceptance criteria

- A Sol review dispatched from a Codex-hosted session either runs on the fast tier with no unknown-key or unadvertised-tier warning, or the tracked documentation states plainly that it does not and why
- `.codex/agents/fable.toml`, `opus.toml`, and `sonnet.toml` are byte-unchanged
- `.codex/codex-review.ps1` is byte-unchanged

## Notes

The 2.5x-for-1.5x cost trade and the reviewer-only scoping are the user's decisions from the session that recorded this Plan; do not revisit them, and do not broaden fast tier to other roles without new user direction.

### Session findings (2026-08-21)

Preparation settled this document's open fact: the installed Codex CLI (v0.148.0) honors a `service_tier` key in per-agent role TOMLs under `.codex/agents/`, because its role-config strings list `service_tier` alongside `model_reasoning_effort`, and a role's tier takes precedence over a tier supplied in the spawn request. The CLI's `models_cache.json` also shows that `gpt-5.6-sol` advertises the fast tier, so the Design's "Supported" branch is the expected outcome. The one thing still unproven is that a role-file tier overrides a user-level `~/.codex/config.toml` setting of `service_tier = "default"`. Settling that requires an observed run, and that run must produce positive evidence the request actually used the fast tier rather than merely showing no warnings.
