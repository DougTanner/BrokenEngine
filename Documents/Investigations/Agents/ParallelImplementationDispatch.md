# Parallel implementation dispatch

## Question

Does dispatching disjoint implementation slices in parallel produce a net
benefit after duplicate discovery, briefing, propagation, and reconciliation
work, and what boundary would make a change to Change Workflow Step 3 safe?

## Current evidence

Root `AGENTS.md`, Change Workflow Step 3, asks main to split disjoint slices and
dispatch implementers in parallel where possible. `/implement-plan` requires a
self-contained brief, a same-context audit, and an affected-site/propagation
handoff. The current contracts do not provide a measured latency corpus or a
duplicate-discovery/reconciliation ledger, so changing the dispatch rule from
one experience would be speculative.

## Options to compare

1. Keep the current rule and collect measurements only. This preserves the
   existing safety boundary while establishing a baseline.
2. Use bounded parallel dispatch only when file ownership and invariants are
   mechanically disjoint, with a manager reconciliation checkpoint. This may
   reduce wall time but retains briefing and merge coordination costs.
3. Use adaptive dispatch based on measured slice size and dependency shape.
   This could capture more wins but introduces a more complex policy and more
   opportunities for incorrect disjointness classification.

## Evidence to collect

- A fixed corpus of comparable multi-slice changes with slice count, discovery
  time, brief preparation, implementation time, propagation, review/fix loops,
  reconciliation, and wall/active time.
- Duplicate discovery and duplicate review evidence, including repeated file or
  symbol searches and conflicts that serial execution would have avoided.
- The effect of parallelism on scope ownership, affected-site propagation,
  acceptance evidence, and the ability to preserve the current one-worker
  same-context audit.
- A safety boundary for disjointness: shared headers, serialization, wire,
  determinism/CRC, build/bootstrap, and cross-subsystem integration must be
  treated as dependent until evidence proves otherwise.

## Promotion criteria

Promote to a Tier 3 workflow Plan only when the corpus shows latency benefit
after duplicate discovery/briefing/reconciliation costs, the disjointness rule
is enforceable, and acceptance/review evidence remains complete. The Plan must
state whether Step 3 changes, what evidence is required before dispatch, and
how the manager reconciles closed handoffs. A neutral or incomplete result
keeps the existing rule and records the evidence gap.

## Non-goals

- No change to Step 3, role counts, concurrency limits, or delegation prompts
  is authorized by this investigation.
- No model-transition, credential, worktree-containment, build, or runtime
  change.

## Notes

The question is latency versus total workflow cost, not whether parallel tools
can be launched. Required reviews, builds, propagation, and landing controls
remain part of the measured work.
