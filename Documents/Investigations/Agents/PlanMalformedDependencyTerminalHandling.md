# Plan Malformed Dependency Terminal Handling

Status: Open investigation; no terminal behavior has been chosen.

Area: Agents / WorktreeCli

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CAI/shard-0061/002` in the frozen C++ adversarial audit.

Frozen audit commit: `76d303f0eeeb86c1ed241edc81634e60070ba5a5`

## Finding under investigation

The candidate is anchored at `Tools/WorktreeCli/PlanScheduler.cpp:968-976`,
inside `RunTerminal`. The loop recognizes a direct dependency child only when
the parsed `plan.dependencies` vector contains the target. A malformed marker
or malformed JSON causes `ParsePlanBytes` to return false before a complete
dependency vector is available (`PlanMetadata.cpp:72-129`), while `BuildPlans`
still retains the tracked path in its `rPlans` map (`PlanMetadata.cpp:172-201`).

For a tracked child whose malformed `dependsOn` text names the target, the
child therefore does not satisfy the loop's `std::find` condition. The
`!plan.bValid` state-conflict branch at `PlanScheduler.cpp:974-976` is not
reached. `RunTerminal` can continue to delete the valid target
(`:994-1007`) and print `completed` (`:1012-1014`) while the invalid child
still carries the raw dependency edge. If that marker is repaired later, the
stale edge can become a misleading dependency on a target that no longer
exists.

## Conflicting scheduler constraints

`Tools/WorktreeCli/AGENTS.md:26` says scheduler final operations must stop with
a state conflict when a dependency child carries invalid metadata. The
selection rules in `Tools/WorktreeCli/AGENTS.md:20` and
`Documents/Plans/AGENTS.md:13` say invalid metadata excludes only the affected
Plans component and unrelated valid Plans remain claimable. The unresolved
policy question is how terminal preparation can fail closed for an invalid
child without turning every unrelated invalid Plan into a global block.

The invariant to preserve is: no successful `complete` or `reject` may delete a
target while a tracked direct dependency child remains unprocessed because its
metadata is invalid. The separate selection invariant is: an invalid Plan that
cannot affect the requested target must not unnecessarily prevent unrelated
valid Plans from being selected or operated on.

## Open alternatives

These are alternatives for a future user decision. Their order is not a
recommendation, and no behavior is selected here.

1. **Target-scoped raw-token quarantine.** Retain or inspect enough raw marker
   information to determine whether a malformed child may name the requested
   target. Quarantine the terminal operation with a state conflict when the
   target relationship is established or cannot be ruled out, while leaving
   unrelated invalid Plans out of the target's operation. The decision must
   define normalization, quoting/escaping, duplicate, and partially parsed
   dependency cases.
2. **Global invalid-plan block.** Refuse terminal preparation whenever any
   tracked Plan has invalid metadata. This gives a simple fail-closed rule but
   changes the current affected-component/unrelated-Plan isolation behavior;
   the user must decide whether that change is acceptable for terminal
   operations.
3. **Another parser-authoritative mechanism.** Change the parser's contract to
   expose an explicit partial dependency state, raw dependency tokens, or
   another authoritative relation result that `RunTerminal` can consume. The
   decision must specify what “unknown” means and when it produces a conflict,
   without guessing a dependency that the parser has not established.

## Decisive questions and evidence needed

- For a malformed child whose raw marker names the target, must `complete` and
  `reject` return the documented state-conflict result (including its exit
  code) before deleting the target or rewriting any child? What exact result
  code and diagnostic are part of the contract?
- For a malformed child that is unrelated to the target, may the terminal
  operation complete while the child remains reported as invalid? This answer
  must be reconciled with the rule that unrelated invalid Plans do not block.
- Which malformed forms count as an established or possible target relation:
  trailing commas, invalid JSON around an otherwise visible path, noncanonical
  paths, escaped strings, duplicate dependencies, and malformed array elements?
  The parser or chosen quarantine mechanism must define this without silently
  dropping a possible edge.
- Does the chosen policy apply identically to `complete` and user-authorized
  `reject`, and does it preserve valid child rewrites, stale-edge notices,
  atomic temporary-file cleanup, and the existing target-existence check?
- What command-level evidence settles the policy? A fixture matrix should
  capture the pre-operation bytes, diagnostics, exit code, result status and
  changed paths, and post-operation bytes for (a) a malformed direct child,
  (b) an unrelated malformed Plan, and (c) normally valid direct children.
  The direct-child case must show that no forbidden target/child mutation is
  published before the conflict.

No parser or scheduler behavior is selected by this record, and no source fix
is authorized by it.

## Earning an executable Plan

After a user chooses the relation-detection policy, malformed-input semantics,
and the required terminal result, this investigation can be converted into a
decision-complete Plan under `Documents/Plans/Agents/`. That Plan must carry
the required byte-zero scheduler metadata, identify the exact parser and
terminal regions in scope, state the isolation boundary for unrelated invalid
Plans, and define command-level acceptance evidence for both terminal commands.
If the choice changes the scheduler authority text, that documentation change
must be explicit in the Plan's scope. Until those decisions exist, this record
remains reference material and WorktreeCli must ignore it.

## Provenance

- Frozen audit report: `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0061.md`
- Candidate: `CAI/shard-0061/002` — “malformed dependency-child metadata is silently left stale while the target is completed”.
- Governing authorities read for this record: `Tools/WorktreeCli/AGENTS.md`,
  `Documents/Plans/AGENTS.md`, `Tools/WorktreeCli/PlanMetadata.cpp`, and
  `Tools/WorktreeCli/PlanScheduler.cpp`.
- No option above is a chosen behavior; no source, build, or scheduler
  operation is part of this investigation.
