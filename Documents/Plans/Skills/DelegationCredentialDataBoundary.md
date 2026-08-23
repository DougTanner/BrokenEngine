<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-23T18:10:18.266Z","dependsOn":[]} -->
# Credential-safe delegation data boundary

## Context

At the session baseline `613687a376bde229f3d734112161dcaeb2d977fc`, the shared
delegation contract lists the objective, scope, repository paths, identity,
acceptance checks, and prohibitions for a brief
(`.agents/references/subagent-reporting.md`, `## Task brief`), but it does not
define which credential-bearing data may cross that boundary.
`.codex/codex-review.ps1` already refuses an
inherited `OPENAI_API_KEY` when it launches a review, yet the general brief
contract has no equivalent rule for other keys, tokens, cookies, passwords, or
authorization material. No credential value is recorded as leaked; the
accepted gap is the missing, reusable trust-boundary rule.

The gap is pre-existing and outside the active implementation boundary. Its
originating criterion is the required delegated brief and the trust rules that
keep worker context limited to its assignment; current routing works, but the
data classes and escalation path are unspecified.

## Design

The recommended contract is:

- Never place a credential value in a brief, prompt, scope file, handoff, or
  receipt. This includes passwords, API keys, bearer tokens, cookies, private
  keys, authorization headers, and raw secret-bearing environment values.
- When a credential-backed operation is necessary, carry only a redacted
  purpose and source, such as the operation being requested and the named
  source class. The worker receives no literal secret and no unredacted
  credential-bearing record.
- Each delegation declares the conditionally allowed and forbidden data
  classes for that assignment. Allowed classes are limited to the minimum
  non-secret metadata needed to perform the assigned check; forbidden classes
  remain forbidden even when a source contains them.
- A worker that needs an additional class returns the request to the manager;
  it does not widen its own brief or ask a tool to expose the data.
- Do not strip the entire environment as a substitute for classification.
  Preserve existing environment behavior and filter only the named forbidden
  classes at the boundary.

## Critical files

- `.agents/references/subagent-reporting.md` — task-brief data classes,
  escalation, and handoff boundary.
- `.agents/skills/implement-plan/SKILL.md` — implementation brief inputs and
  worker prohibitions.
- `.agents/skills/codex-review/SKILL.md` — delegated review prompt and untrusted
  input boundary.

## In scope

- Add the credential-value prohibition, redacted purpose/source form,
  conditional data-class declaration, manager-only expansion route, and
  narrow-filter/no-broad-environment rule to the three critical files' brief,
  prompt, and trust sections.
- Add read-only positive and negative review scenarios that prove a redacted
  purpose survives, a credential value is rejected, and an expansion request
  returns to the manager.
- Preserve the existing role routing, read-only review sandbox, and current
  inherited-key refusal.

## Out of scope

- Authentication, credential storage or retrieval, plugin installation, service
  authorization, or changes to user environment configuration.
- Broad environment sanitization, secret scanning of unrelated repository
  history, or a new credential broker.
- Model selection, routing parity, review receipt versioning, or landing-lock
  behavior.
- Unit tests, builds, runtime game behavior, determinism/CRC, replay, wire,
  serialization, data layout, or shaders.

## Risk tier and invariants

Expected future Change Workflow Tier 3 — delegation trust-boundary behavior.
The trigger is data crossing from manager context into worker prompts and
headless review execution. Credential values must never cross that boundary;
redacted purpose/source must be sufficient for routing; conditional expansion
must return to the manager; and no broad environment stripping may silently
change unrelated tool behavior.

## Acceptance criteria

- A generated brief, review prompt, and handoff contain no credential value;
  positive evidence shows only the redacted purpose/source form.
- Each conditional operation states its allowed and forbidden data classes, and
  a worker request for a missing class returns to the manager without widening
  the assignment.
- The negative scenarios reject passwords, keys, tokens, cookies, private keys,
  authorization headers, and raw secret-bearing environment values without
  printing them.
- The existing inherited-key refusal remains intact and no broad environment
  stripping is added.
- The trust-boundary review and static link/reference checks pass; Plan
  validation exits `0` with `status: valid` and `code: ok`.
- No unit tests are added.

## Coordination

- Coordinate wording with `Documents/Plans/Agents/CodexRoutingParityAndReviewProvenance.md`
  if its receipt changes also carry data-class metadata. Neither Plan depends
  on the other; reconcile the shared trust vocabulary before either change is
  reviewed.

## Notes

The current `OPENAI_API_KEY` launch refusal is evidence for a narrow existing
guard, not permission to generalize to blanket environment removal. The future
implementation must keep the worker's assignment useful while ensuring that a
credential value is never used as task context.
