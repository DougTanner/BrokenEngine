# Isolated interface prototype worktrees

Revisit When: A read-only interface proposal cannot settle a design because
candidate compilation needs isolated checkout state, and a Tier-3
workflow/tooling owner is available to coordinate prototype worktrees and
their disposal.

## Context

The current `/external-design-interface` workflow is Option 1: proposals are
read-only and do not create source, build output, branches, or worktrees. This
Feature records Option 3 as an independent future capability for stronger
compile evidence. It does not alter the current skill, and it explicitly
excludes the ignored-scratch design in `ScratchInterfacePrototypes.md`.

## Decision-complete design

The manager captures one fixed baseline identity, fixes candidate ownership, and
supplies the exact allowed worktree and branch creation scope. It then
dispatches a coordinator implementer, which creates exactly three isolated
prototype worktrees and corresponding branches from that baseline, one for
each candidate. The coordinator materializes the run manifest with the exact
absolute worktree path, exact branch reference, candidate index, owner, and
baseline before the manager routes a candidate to its implementer. Each
candidate implementer owns only its assigned worktree; planners remain
read-only design producers; no implementer edits another candidate or the
primary checkout, and no candidate rebases or changes the fixed baseline.

Each candidate implementer edits the affected tracked-shape source and project
membership in its own worktree. The manager dispatches the existing `/compile`
builder workflow inside that worktree: target/configuration follows candidate
affinity, using Client or Server Debug as appropriate; shared interfaces
compile under both affinities. The builder is the sole compiler owner and
returns a structured result tied to that worktree, branch, candidate, and
baseline. The manager semantically owns the comparison evidence, user
interaction, exact user selection, and selected-design handoff. The coordinator
implementer materializes one comparison packet within the manager-supplied
run scope and is its only filesystem writer; the manager routes candidate
observations and the exact selection to it. Before disposal, after the manager
confirms the handoff is complete, the coordinator materializes the selected-design evidence
and exact choice at the manager-supplied implementation-plan scope.

On success, after the manager confirms the handoff is complete, the coordinator
implementer disposes only the three worktree paths and branch references
recorded in the manifest under the manager-supplied scope, then records a
receipt proving each exact target is gone. It never merges, lands, or adopts a
prototype worktree as an implementation session. On any worker, builder,
selection, handoff, or disposal failure, the coordinator attempts disposal of
each already-created exact path and branch in the manifest. It reports every
target that remains and preserves a failure receipt; the manager blocks
implementation and primary landing and routes an explicit recovery/cleanup
action for that residue. No broad worktree prune or branch deletion is allowed.

Because the mode coordinates worktrees, builds, and landing-adjacent cleanup,
implementing it requires a future Tier-3 workflow/tooling change. The current
read-only workflow remains authoritative until that separate change is
approved and implemented.

## Critical files and surfaces

- `.agents/skills/external-design-interface/SKILL.md` — current read-only seam
  and future opt-in boundary.
- `.agents/skills/compile/SKILL.md` — repository-supported builder contract.
- `AGENTS.md` — fixed-baseline, worktree, build, and landing authority.
- `Documents/Features/AGENTS.md` — manual Feature lifecycle.
- Coordinator implementer mutation surface — manager-supplied exact worktree,
  branch, run, manifest, packet, and disposal scopes; it never targets primary.
- Git worktree and branch state — exact manifest targets for creation,
  disposal, and recovery; never primary landing targets.

## In scope

- A separately approved workflow mode that creates exactly three isolated
  prototype worktrees and branches from one fixed baseline.
- One owner per candidate, coordinator-owned worktree/branch and lifecycle
  materialization under manager-supplied scope, builder-owned compilation inside
  each worktree, and manager-owned comparison semantics, user interaction,
  selection, routing, and selected-design handoff.
- Exact-target success disposal, failure recovery, residue receipts, and
  safeguards against primary landing or session adoption.

## Out of scope

- Changing the current Option 1 read-only skill behavior.
- Option 2 ignored scratch prototypes; that design is owned by
  `ScratchInterfacePrototypes.md` and is explicitly not combined here.
- Merging, landing, or adopting a prototype worktree as an implementation
  session; implementation starts separately after the handoff.
- New engine interfaces, compiler commands, build/bootstrap policy, external
  services, or credentials.

## Risk tier and invariants

Future implementation is Tier 3 because worktree creation/disposal, candidate
compilation, and landing-adjacent coordination cross workflow ownership
boundaries. The implementation must preserve these invariants:

- All three candidates use the same fixed baseline, and each worktree and
  branch has exactly one recorded owner.
- Candidate implementers write only their assigned worktree. Planners remain
  read-only design producers; the primary checkout, primary branch, and
  unrelated worktrees remain untouched.
- The builder is the sole compiler owner and binds every result to its exact
  candidate worktree, branch, and baseline.
- The manager owns comparison semantics, user interaction, exact user choice,
  selected-plan handoff, and routing. The coordinator implementer materializes
  the manifest, comparison packet, and selected-plan handoff only within
  manager-supplied scopes. No implementation starts before that handoff is
  complete.
- Coordinator disposal and recovery operate only on manifest-recorded exact
  paths and branch references within manager-supplied scope. Success proves no
  targets remain; failure reports residue and the manager blocks implementation
  and landing.
- This mode never adopts a prototype worktree as an implementation session and
  never activates Option 2 scratch behavior.

## Acceptance criteria

- A coordinator implementer creates exactly three prototype worktrees and
  branches from one manager-supplied fixed baseline/scope, with one owner and
  exact path/ref recorded for each before candidate dispatch.
- Each candidate implementer edits only its assigned worktree, and the existing
  `/compile` builder workflow compiles that candidate inside the worktree with
  target/configuration chosen by affinity; shared interfaces compile under both
  Client and Server affinities, with auditable results tied to the manifest.
- The coordinator-materialized packet contains the manager-owned semantic
  comparison evidence: declarations, usage, callers/invariants, trade-offs,
  compile/static observations, and exact user selection; the coordinator-
  materialized selected-design handoff is present in the plan before disposal.
- After manager confirmation, the coordinator disposes only the three manifest
  targets and records a passing receipt. A failed disposal/recovery reports
  every remaining exact target and the manager prevents implementation or
  primary landing.
- Primary branch state is unchanged, and no prototype worktree is adopted as
  the implementation session.

## Notes

This is a manual Feature, not an executable Plan. Its future implementation
requires a separately approved Tier-3 workflow/tooling stage because worktree,
build, and landing-adjacent coordination are authoritative. It explicitly
excludes ignored scratch mode and does not authorize any current-session
mutation.
