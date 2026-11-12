<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-11T19:21:32.776Z","dependsOn":[]} -->
# Decide the Default Wrapping for Human-Readable Prose

## Context

The user requested a follow-up to decide a prose wrapping default after a
documentation reflow made SmartGit diffs harder to read. The author records the
user's preference for the prior shorter wrapping and their decision to defer
choosing a default. Restoring the current reflow belongs to the originating
change, outside this follow-up.

The current policy supplies a maximum, without a preferred wrapping target:
`.agents/scripts/Invoke-StaticChecks.ps1:7-10` describes the changed-file scope
and exceptions; `MaximumLineLength` owns the ceiling; `Get-LineLengthFailure`
enforces it. `.agents/references/change-workflow.md`, Step 5, requires repairs
for reported failures. `.agents/skills/update-claude-docs/references/worker.md`,
step 6, delegates the limit to that runner. `.editorconfig`, the Markdown/text
section, specifies trailing-whitespace handling without a wrapping default.

The confirmed gap is an absent deliberate prose default, rather than a runtime
defect or a requirement to reflow lines toward the runner's maximum. This task
is recorded as a Plan at the user's explicit request, with the choice reserved
for the executing session's user discussion.

## Design

At execution, compare preserving existing wrapping, a consistent short wrap,
and semantic line breaks using a small representative prose edit and its
SmartGit diff. Present the tradeoffs for source readability and diff locality;
obtain the user's choice before changing the default. Do not infer that the
static-check ceiling is the desired line width.

After that choice, document the smallest agreed default at its owning
instruction layer, linking from dependent workflow instructions only where
needed. The author's recommendation is to keep enforcement unchanged and use
instruction prose for the agreed default, avoiding unnecessary tooling. Any
change to the ceiling or its exceptions requires an explicit execution-time
user decision and reclassification before implementation.

## Critical files

- `AGENTS.md` — repository-wide prose instruction, if selected as owner.
- `.agents/references/change-workflow.md` — Step 5 rewrapping instructions.
- `.agents/skills/update-claude-docs/references/worker.md` — runner-owned limit
  reference, only if omission would conflict with the chosen instruction.
- `.agents/scripts/Invoke-StaticChecks.ps1` — existing ceiling and exceptions;
  inspection and verification only under the default scope.
- `.editorconfig` — inspect the existing Markdown/text section.

## In scope

- Compare the three wrapping approaches and settle the default with the user.
- Define the chosen approach's applicability to human-readable Markdown prose
  and top-level Documents text guides, including handling existing wrapping
  during ordinary edits.
- Update the owning instruction and only directly conflicting references.
- Verify a representative resulting diff and run the existing static checks.

## Out of scope

- Executing this decision in the originating session or restoring its reflow.
- Repository-wide reflow, unrelated documentation cleanup, new formatters,
  editor integrations, or automated wrapping checks.
- Changing the static-check ceiling, fence/table/metadata exceptions, code
  wrapping, runtime behavior, or asset formats without a new user decision.

## Acceptance criteria

- The executing user has chosen an approach after seeing representative diffs;
  the record distinguishes that choice from the author's recommendation.
- One owning instruction states the default and treatment of existing prose;
  dependent instructions agree without duplicating the policy.
- The representative edit produces the agreed readable diff, and changed
  instruction files pass the existing applicable static checks.
- The current ceiling and exceptions remain intact unless the executing user
  explicitly approves changing them through a revised scope.

## Notes

Expected Change Workflow tier: Tier 1, triggered by instruction-only
documentation with no tool-behavior or invariant changes. Reclassify if the
executing user expands scope into checking behavior. No build, unit tests, or
live harness verification is required. There is no determinism/CRC,
serialization, replay, wire, affinity, threading, allocation, or shader
exposure. Dependencies and Coordination: none.
