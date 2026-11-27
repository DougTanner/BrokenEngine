<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:32:29.151Z","dependsOn":[]} -->
# Dispatch a caller-scoped `/comment-review` without a step-script row

## Context

`.agents/skills/comment-review/SKILL.md` `## When to use` supports two uses:
the Change Workflow Review and resolve correctness step, and a comment cleanup
sweep over a scope the caller supplies (`WholeFile` mode,
`references/worker.md` step 1). The paragraph after that list says to run as
one `mechanic` and that "Dispatch is once per printed step-script row".

The step script prints a `/comment-review` row only for a round whose diff
triggers it (`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header, review keys
and since points; the trigger is derived from changed C++ or GLSL in
`.agents/scripts/SessionChangeInventory.psm1` `Get-RoutingTrigger`). A
caller-supplied cleanup of unchanged files has no printed row, so a dispatcher
following that sentence has no row to dispatch from. Observed: a Tier-2
landing preview printed no `/comment-review` row, while
`Find-CommentBlocks.ps1 -Path Common/WindowsUtils.h` scanned that unchanged
file and returned its blocks.

The sentence came with the landed review-round change (commit `94fa96136`).

## Design

Limit the printed-row sentence to Change Workflow reviews, and state that a
caller-supplied scope is dispatched directly as one `mechanic`.

Change Workflow tier: Tier 1 — documentation: one dispatch sentence in a skill
body. No behavior, determinism, wire, or trust-boundary change; no C++, shader,
build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/skills/comment-review/SKILL.md` (the dispatch paragraph after
  `## When to use`)

## In scope

- That paragraph's dispatch sentence.

## Out of scope

- `## Inputs`, the worker, and the scanner.
- The step script's rows and triggers.

## Acceptance criteria

- `SKILL.md` ties once-per-printed-row dispatch to Change Workflow reviews and
  names direct dispatch of one `mechanic` for a caller-supplied scope.
- `Validate-Skill.ps1` returns `VALID` for `comment-review`.
- The static-checks runner passes for the changed files.
