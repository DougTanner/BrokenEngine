<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T12:05:39.754Z","dependsOn":[]} -->
# Fix: delegated handoffs — child execution metadata is purged before model/effort routing can be proven

## Context

Observed symptom, from a `/next-plan-review` of an earlier landed session: 12 of
that session's 13 ordinary subagent children had their task output artifacts
truncated to 0 bytes once the agent completed. Only one long-lived resumed
finalizer still had its metadata, which proved executor `claude-opus-5` at effort
`medium`. Every other routing row therefore had to be reported `unverified`,
even though the session dispatched every child by `subagent_type` with no ad-hoc
model override anywhere.

Current behavior, read from the working tree:

- `.agents/skills/next-plan-review/references/measurement.md:69-77` allows
  exactly one evidence chain for an ordinary Claude child: the parent delegation
  event, the recorded returned child relationship, and child-session execution
  metadata naming the actual executor/model and effort. It states that a
  requested role or configured mapping proves intent only, and that when required
  model or effort evidence cannot be proved the verdict is `unverified`.
- `.agents/references/subagent-reporting.md:50-64` defines the handoff form
  (`Status`, `Changed files`, `Decisive checks`, `Build required`, `Residuals`)
  and carries no executor or effort field.

Because the only accepted actual-model proof lives in host-owned transient files
that the host truncates at child completion, a correctly routed session still
produces a block of `unverified` routing rows, and the cost of that block recurs
in every review.

The misbehaving surfaces are the repository's delegated-handoff form and
`/next-plan-review`'s evidence chain, both outside the `## In scope` boundary of
the Plan the observing session had claimed, so this is tooling friction rather
than an in-scope acceptance failure of the change that session landed.

Verify every cited line number against the working tree before editing — the
numbers above may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a ref
whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1b583079-2807-42e9-930f-2394560b2edc
- Worktree/branch UUID: 9210ba0b-3bb1-4251-80be-0a74eef865cc
- Session branch: claude/9210ba0b-3bb1-4251-80be-0a74eef865cc
- Worktree: .claude\worktrees\BrokenEngine\9210ba0b-3bb1-4251-80be-0a74eef865cc
- Observed in an earlier session: the six fields above are the observing
  session's, not the recording session's. That session's landed commit is
  `44b1259d50eaf4582bae5e25d5e35386abc3619b` ("Accept landing-candidate sessions
  in plan list and rename quarantined to excluded"), a single-session landing
  commit whose tree contains that session's work.
- Landing ref: `claude/a9c4f4cc-6cd6-4bf1-8511-9702c6308d1f`, the recording
  session's branch, whose tip is that session's final commit. Fallback once that
  ref is gone: `git log --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.

## Design

The `/next-plan-review` that produced this residual has already run and proved
the symptom above, so no further transcript review is required before
implementing. The provenance block is retained only so an implementer who needs
more context can reach the observing session while it is still available.

Move the proof into the artifact that survives child completion — the handoff:

1. In `.agents/references/subagent-reporting.md` `## Handoffs`, add one required
   line to the handoff form naming the worker's own executor model identifier and
   effort as the host reports them to that worker, with `unknown` when the worker
   cannot read either value. Keep `Build required` present and `Residuals` last,
   and change no other field.
2. In `.agents/skills/next-plan-review/references/measurement.md`, extend the
   ordinary-Claude-child evidence chain so a row may also be proved by the parent
   delegation event, the recorded returned child relationship, and that handoff
   line, when host-owned child metadata is unavailable. Such a row is `compliant`
   and cites the handoff line as self-reported. A missing or `unknown` line keeps
   the row `unverified`.
3. Leave the headless `/codex-review` evidence chain exactly as it is; it is
   already proved by the wrapper invocation and the commit-time model/effort pins.

Accepted trade-off, recorded so it is not re-opened: a worker's self-report is
weaker than host metadata, because it is the routed model describing itself. It
is accepted because it is the only artifact that outlives the child, and it still
exposes the case this concern exists to catch — the model that actually ran
differing from the role mapping.

Deliberately rejected alternative: retaining the host's child metadata header.
The host owns those files and the repository cannot change their lifetime.

## Critical files

- `.agents/references/subagent-reporting.md` — the `## Handoffs` form (`:50-64`)
- `.agents/skills/next-plan-review/references/measurement.md` — the allowed
  evidence chains and verdict rules (`:69-89`)
- `.agents/skills/next-plan-review/SKILL.md` — the `Execution-model routing`
  inventory row (`:242`) and its report line (`:229`), edited only if the added
  evidence source makes their wording inaccurate

## In scope

- The `## Handoffs` form in `.agents/references/subagent-reporting.md`: one added
  required executor/effort line
- The ordinary-Claude-child evidence chain and its verdict rule in
  `.agents/skills/next-plan-review/references/measurement.md`
- The `/next-plan-review` routing row and report line, only where the added
  evidence source would otherwise leave them inaccurate

## Out of scope

- The root `AGENTS.md` delegation role table and routing policy, and the role
  definitions under `.claude/agents/` and `.codex/agents/`
- The headless `/codex-review` evidence chain and `.codex/codex-review.ps1` pins
- Every other handoff field, the task brief, and the interruption/recovery rules
- `/next-plan-review`'s other concerns: control-work share, token efficiency,
  process overhead, and speed
- Any change that makes a self-reported value substitute for host metadata where
  host metadata is present
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior: the delegated-handoff form and one
skill's evidence chain); escalate to Tier 3 if the fix reaches the root
`AGENTS.md` routing policy or role definitions. Invariants to preserve: intent
evidence — requested role, requested model, configured mapping — never proves a
route on its own; the headless chain is unchanged; and an absent or `unknown`
executor line still yields `unverified` rather than an inferred `compliant`.

## Acceptance criteria

- Every delegated handoff carries the executor/effort line, or `unknown` when the
  worker cannot read it
- A `/next-plan-review` routing row for an ordinary Claude child can reach
  `compliant` from the handoff line once host-owned child metadata is gone, and
  still reports `unverified` when the line is absent or `unknown`
- The headless `/codex-review` evidence chain is unchanged
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the (artifact, symptom) pair: the delegated-handoff form
carries no executor/effort field, so `/next-plan-review` routing rows for
ordinary Claude children are `unverified` once the host purges child metadata. A
later observation of the same pair is a duplicate, not a new residual.
