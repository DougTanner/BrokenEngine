<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-24T22:48:03.219Z","dependsOn":[]} -->
# Refresh the BuildAndDispatchFrameTicks allocation exemplar

## Context

The allocation-tracked guidance in `.agents/skills/repo-code-review/SKILL.md:146-148`
currently says to follow `GameBase::BuildAndDispatchFrameTicks` for a
"scoped workbuffer-backed list whose lifetime covers synchronous dispatch."
That exemplar is stale. The current implementation at
`Engine/Source/GameBase.cpp:456-485` rebuilds the persistent
`mActiveFrameRefs` member under `ScopedSuppressAllocationTracking`, then keeps a
reference to that vector while dispatching the frame ticks. The owning member
and its cross-tick capacity contract are declared at
`Engine/Source/GameBase.h:293-294`.

The root cause is a documentation gap: the implementation moved from a
workbuffer span to the reusable member vector, but the review skill's exemplar
was not updated. A reviewer following the old sentence could recommend the
wrong temporary-allocation pattern or miss the required suppression rationale.
The gap was recorded by the implementation affected-site note and the Tier-3
adversarial review. It is unresolved in the current tree: the skill file is
unchanged and is absent from this session's approved changed-file list.

This is outside the active change's documentation boundary. That boundary
names the Frame subsystem AGENTS files, `FrameUpdatePipeline.md`, the
AgentHarness commands and implementation, and deletion of the registry-query
investigation; it does not include `.agents/skills/repo-code-review/SKILL.md`.
The current implementation and acceptance evidence do not depend on changing
the review skill, so this is follow-up documentation debt rather than an
in-scope acceptance failure.

## Design

Recommendation: revise only the allocation-tracked guidance around
`.agents/skills/repo-code-review/SKILL.md:146-153` so that it keeps the general
`gpThreadLocal->mWorkbuffer` rule for tracked temporary data, describes
`BuildAndDispatchFrameTicks` as rebuilding the persistent `mActiveFrameRefs`
vector under its scoped allocation-suppression guard for synchronous dispatch,
and retains the `CrcValidateLoop` workbuffer-arena exemplar for loop-built
temporary log text. This is the smallest wording change that makes the skill
agree with the current source and explains why the persistent vector is
allowed to grow.

Do not change `GameBase`, the workbuffer, allocation-tracking behavior, or any
other review rule. After the wording edit, run `/validate-skill` for the
changed skill and `/progressive-disclosure-review` for the resulting skill
prose, then run Plan validation.

## Critical files

- `.agents/skills/repo-code-review/SKILL.md:139-153` — the allocation-tracked
  paths and logging guidance; this is the only intended edit surface.
- `Engine/Source/GameBase.cpp:456-485` — current
  `BuildAndDispatchFrameTicks` behavior, read-only evidence for the wording.
- `Engine/Source/GameBase.h:293-294` — persistent `mActiveFrameRefs` member,
  read-only evidence for its lifetime and capacity contract.

## In scope

- Update the stale `BuildAndDispatchFrameTicks` exemplar in
  `.agents/skills/repo-code-review/SKILL.md:146-153` to describe the current
  persistent-vector and scoped-suppression behavior.
- Run `/validate-skill` on the changed `.agents/skills/repo-code-review/SKILL.md`
  and `/progressive-disclosure-review` on the resulting skill prose.
- Run `WorktreeCli plan validate` and require the versioned valid/ok result.

## Out of scope

- All source, architecture, harness-command, and investigation changes from
  `registry-query-window-safe-growth-v2`, including `GameBase.cpp` and
  `GameBase.h`.
- Any redesign of `gpThreadLocal->mWorkbuffer`, `mActiveFrameRefs`,
  `ScopedSuppressAllocationTracking`, or allocation-tracking behavior.
- Other sections or skills, unrelated review guidance, new compatibility
  behavior, unit tests, builds, harness scenarios, commits, claims, or landing.

## Risk tier and invariants

Expected Change Workflow Tier 1 — mechanical documentation. The trigger is a
single wording correction in a tracked skill with no runtime behavior, public
signature, data layout, or invariant change. Escalate if implementation grows
to source behavior or any build/bootstrap coordination.

No determinism/CRC, serialization/`.pack`/`kiVersion`, replay, wire, threading,
shader, build, or live-verification surface changes. The wording must remain
faithful to the existing allocation boundary: the persistent vector may grow
under its scoped `ScopedSuppressAllocationTracking` guard, while genuinely
temporary tracked data continues to use the workbuffer guidance.

## Acceptance criteria

- The allocation-tracked section no longer calls
  `BuildAndDispatchFrameTicks` a scoped workbuffer-backed list and instead
  accurately describes `mActiveFrameRefs` and its suppression/lifetime model.
- `/validate-skill` passes for `.agents/skills/repo-code-review/SKILL.md`.
- `/progressive-disclosure-review` reports no layering or duplicated-guidance
  issue for the changed skill prose.
- `WorktreeCli plan validate` exits 0 with `status: valid` and `code: ok`.

## Notes

No dependency edge or reciprocal Coordination section is needed: this Plan
owns one documentation correction and has no directional prerequisite or
cross-Plan coordination.
