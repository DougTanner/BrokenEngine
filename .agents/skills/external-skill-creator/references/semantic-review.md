# Skill Validation Procedure

Steps and rules for the fresh reviewer selected by [`worker.md`](worker.md).
Public contract, inputs, and handoff form: [`../SKILL.md`](../SKILL.md).

## Steps

1. Read [`frontmatter-schema.md`](frontmatter-schema.md) completely before
   interpreting frontmatter or a mechanical diagnostic. It is the authoritative
   repository schema; do not substitute a client-installed validator or a
   reduced prose check. Done when the whole schema has been read.
2. Bootstrap the validation boundary by running the mechanical command against
   this `external-skill-creator` directory:

   ```powershell
   pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/external-skill-creator
   ```

   Require `VALID` and exit `0`. Report `BLOCKED` if the command cannot run,
   returns `SETUP_ERROR`/2, or the validator does not validate its own skill. Do
   not continue with a weaker check.
3. Run the same command once for the target, using `-Fixture` only for a
   deliberately disposable fixture outside `.agents/skills/`. Done when the exact
   command, exit status, and complete output are captured.
4. Classify the target result. `INVALID`/1 is a mechanical Critical finding;
   `SETUP_ERROR`/2, an unrecognized result class, or a result/exit mismatch is
   `BLOCKED`.
5. Review semantic behavior from `SKILL.md`, directly referenced resources, and a
   present Codex companion file. Classify each compatibility conclusion as
   structural acceptance, loader acceptance, documented behavior, or observed
   runtime behavior. Done when each surface has a verdict and every
   compatibility claim names the evidence class that supports it.
6. Collect inbound references by running `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1 -SkillName <name>`,
   which sweeps the documented root set and returns capped `{path, line, text}`
   records with per-root hit counts; never reconstruct that sweep inline.
   Complete a `truncated` result with targeted searches of the roots whose
   reported counts exceed the returned records. Report the blocker on error
   (exit `1`). Done when every collected reference is classified: model
   invocation or chaining is a workflow requirement, and a command only the user
   types is an example.
7. Return the handoff form in [`../SKILL.md`](../SKILL.md). Done when every
   declared field is filled and returned.

## Rules

- Treat Claude `disable-model-invocation` and Codex
  `policy.allow_implicit_invocation` as independent controls. Never require a
  Codex companion file merely because the Claude flag is true. A
  platform-neutral or Codex-specific manual-only promise requires Codex policy
  `false`; a Claude-only promise requires the Claude flag. A disabled policy
  that conflicts with an inbound workflow requirement is Critical for that
  client.
- Critical: `description` lacks meaningful trigger contexts. Codex discovery
  sees `name` and `description`, not `when_to_use` or the body; package listing
  or loader acceptance does not prove implicit invocation.
- Treat structural validation as evidence of repository-schema acceptance
  only, and loader validation as evidence that a client parsed and listed the
  package only. Use current authoritative documentation for documented client
  semantics and direct host observation for claims about actual invocation or
  tool behavior. Record unsupported compatibility claims as findings; record
  unavailable runtime behavior as unverified when the target makes no claim
  that depends on it. Do not require a runtime check merely because a package
  contains client-specific metadata.
- Recommended: remove unused Claude pre-approvals and add those needed by
  prescribed commands. Keep body tool and delegation bounds authoritative; Codex
  dependencies wire external tools and grant no execution authority.
- Recommended: keep instructions imperative, general, and concise; include an
  example for a non-trivial required output format.
- Recommended: plain wording — apply the one-term-per-concept and plain-words
  rules in root `AGENTS.md` `## Directives`, and name concrete actions instead of
  abstract noun-stacks unless the stack is an established term or code
  identifier. Report a reintroduced synonym for an established term as a finding.
- Recommended: phrase each rule positively as the action to take; keep a
  prohibition only where the requirement cannot be stated positively, and pair it
  with what to do instead.
- Recommended: end each workflow step on a checkable done-condition, so the agent
  can tell done from not-done.
- Do not promote ordinary quality advice to Critical unless discovery or
  invocation is concretely incorrect.
- Progressive disclosure and body/reference size belong to
  `/progressive-disclosure-review`; report neither here.
- For validator changes, run the disposable `VALID`/`INVALID`/`SETUP_ERROR`
  matrix in [`frontmatter-schema.md`](frontmatter-schema.md). Do not commit
  fixture packages.
