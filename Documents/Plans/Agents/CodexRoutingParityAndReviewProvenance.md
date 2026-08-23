<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-23T18:10:28.125Z","dependsOn":[]} -->
# Codex routing parity and review provenance

## Context

At the session baseline `613687a376bde229f3d734112161dcaeb2d977fc`, the root
role table maps planner/reviewer/implementer/locator/builder work to model and
effort tuples. The Codex role TOMLs repeat those pins, while
`.codex/codex-review.ps1` hard-codes the headless Sol review route and its
medium reasoning effort. The wrapper currently emits
`broken-engine-codex-review/v3` with run identity, output path, exit code,
retry, and reason, but not the prompt digest, launcher identity, CLI version,
requested tuple, or authoritative actual values. The delegated-reporting
contract also records Codex worker model and effort as `unknown` because config
and CLI pins state intent rather than proof
(`.agents/references/subagent-reporting.md`, `## Handoffs`).

The accepted gap is therefore twofold: no deterministic drift check proves
that the authoritative role table, Codex TOMLs, and headless route agree, and a
completed review cannot prove its launch provenance without exposing prompt
contents. This is pre-existing and outside the active implementation
boundary; it is not evidence that a current route is wrong.

## Design

The recommended implementation has four bounded parts:

1. Add a deterministic drift validator that treats the root role table as the
   authority, compares every mapped Codex TOML and headless review pin against
   it, and records the Claude documented exception explicitly. A mismatch
   names the role, field, authoritative value, and observed configured value;
   an unknown actual value remains `unknown` rather than being inferred from a
   pin.
2. Add a changed-route smoke scenario that exercises the route selected by a
   changed role mapping and proves the requested role reaches the expected
   host/model/effort path. The scenario must use the validator's fixture or
   receipt evidence and must not claim a live value from configuration alone.
3. Version the review receipt as v4 with prompt SHA256, launcher path and hash,
   CLI version, requested model/effort/service tuple, and authoritative actual
   values or `unknown`. The receipt never carries prompt contents.
4. Cover a stable fixture digest, identical-prompt retry, prompt-privacy,
   and route-mismatch cases. A malformed final verdict remains a malformed
   review; a retry records that it occurred and retains no prompt bytes in the
   receipt.

The root role table, Codex TOMLs, and headless pin remain separate source
artifacts so the validator can detect drift; do not collapse them into one
generated file or add compatibility versions without user direction.

## Critical files

- `AGENTS.md` — authoritative role/model/effort table and Claude exception.
- `.codex/agents/fable.toml`, `.codex/agents/sol.toml`,
  `.codex/agents/opus.toml`, `.codex/agents/sonnet.toml` — Codex role pins.
- `.codex/codex-review.ps1` — headless launcher and review receipt.
- `.agents/skills/codex-review/SKILL.md` — receipt and fallback consumer.
- `.agents/references/subagent-reporting.md` — unknown-versus-authoritative
  executor provenance wording.
- `.claude/agents/reviewer.md` — documented Claude fallback exception.

## In scope

- Add the deterministic validator for the named role table, Codex role files,
  headless route, and documented Claude exception.
- Add changed-route smoke coverage and the four receipt/privacy fixtures in the
  existing review-tooling test/check surfaces.
- Change the headless receipt from v3 to v4 with the exact provenance fields
  above, preserving the current identical-prompt retry and malformed-verdict
  behavior.
- Update the named consumers and handoff wording to distinguish requested,
  configured, actual, and unknown values without treating a config pin as
  runtime proof.

## Out of scope

- Changing the selected role models, effort levels, service tier, or Claude
  fallback policy; a detected mismatch is evidence for correction inside this
  Plan, not permission to redesign routing.
- Prompt contents, transcript contents, credential values, broad environment
  stripping, or a new review backend.
- Landing-lock implementation, worktree containment, engine/runtime code,
  build/bootstrap coordination, determinism/CRC, replay, wire, serialization,
  data layout, shaders, or unit tests.

## Risk tier and invariants

Expected future Change Workflow Tier 3 — cross-host serialized routing and
review provenance. The trigger is the trust-sensitive relationship between
authoritative role policy, host launchers, and persisted review receipts. The
validator must be deterministic; route changes must be smoke-checked; actual
values must be authoritative or explicitly unknown; prompt bytes must never be
serialized; and identical retries must remain identifiable without changing
the prompt.

## Acceptance criteria

- The validator passes the current role matrix with no drift and reports a
  fixture mismatch with role, field, authoritative value, and configured value
  when one is introduced.
- The changed-route smoke scenario proves the selected route from receipt or
  host evidence, not from a config pin alone.
- A v4 receipt carries prompt SHA256, launcher path/hash, CLI version, requested
  tuple, and authoritative actual values or `unknown`, and contains no prompt
  contents or credential values.
- Stable fixture digest, identical retry, privacy, and mismatch scenarios all
  produce their expected result; the existing malformed-verdict behavior stays
  intact.
- The updated consumer/handoff distinguishes requested, configured, actual,
  and unknown values; static checks and Plan validation exit `0` with
  `status: valid` and `code: ok`.
- No unit tests are added.

## Coordination

- Coordinate receipt trust wording with
  `Documents/Plans/Skills/DelegationCredentialDataBoundary.md` if the v4
  receipt carries data-class metadata. Neither Plan depends on the other;
  reconcile the shared redaction vocabulary before review.

## Notes

The current wrapper's v3 receipt and the role TOMLs are durable evidence of
intent and launch structure, not proof of runtime identity. The future change
must retain that distinction and must not solve provenance by copying prompt or
transcript bytes into a tracked artifact.
