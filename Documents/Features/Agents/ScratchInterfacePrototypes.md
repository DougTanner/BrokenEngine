# Scratch interface prototypes

Revisit When: A read-only interface proposal cannot settle a design because
compile or layout evidence is required, and a Tier-3 workflow/tooling owner is
available to implement the scratch-prototype lifecycle.

## Context

The current `/external-design-interface` workflow is Option 1: proposals are
read-only and do not create source, build output, branches, or worktrees. This
Feature records Option 2 as an independent future capability for cases where a
small prototype is useful without creating a Git worktree. It does not alter
the current skill and it does not combine with the isolated-worktree design in
`IsolatedInterfacePrototypeWorktrees.md`.

## Decision-complete design

The manager fixes the unique run identifier, baseline identity, and candidate
ownership, then dispatches a coordinator implementer with the exact ignored
scope `Temp/InterfacePrototypes/<run-id>/`. The coordinator implementer creates
the run directory and one assigned subdirectory for each candidate
(`candidate-1`, `candidate-2`, or `candidate-3`), materializes the run manifest,
and returns the path and ownership receipt before the manager routes candidates.
Each candidate implementer may create prototype source only below
its assigned directory. Candidate implementers cannot write the other
candidate directories, the manager evidence directory, tracked paths, or any
worktree.
Planners remain read-only design producers; candidate implementers alone
materialize scratch source within their assigned directories.

The manager semantically owns the comparison evidence, user interaction, exact
user selection, and selected-design handoff. The coordinator implementer
materializes the comparison packet at
`Temp/InterfacePrototypes/<run-id>/comparison.md` within the manager-supplied
scope and is its only filesystem writer. The manager routes candidate
observations and the exact user selection to the coordinator, which records,
for every candidate, the interface declaration, representative usage, affected
callers and invariants, trade-offs, compile/static observations, and the fixed
run identity and ownership. After the manager confirms the handoff is complete,
the coordinator materializes it at the manager-supplied plan scope before
cleanup; raw prototype source is never promoted into tracked files.

Candidate workers do not compile. The future implementation adds an opt-in,
compile-only interface-prototype target owned by `/compile` and WorktreeCli.
The delegated builder is its sole caller and passes one exact candidate
translation unit under
`Temp/InterfacePrototypes/<run-id>/<candidate>/`. The target evaluates the same
compiler settings, defines, and includes as the selected Client or Server
Debug target, produces no linked executable, and writes compiler output and its
structured result under that candidate directory. Candidate affinity selects
Client or Server Debug; shared interfaces run both affinities. Authoritative
compilation remains a future Tier-3 workflow/tooling change because adding this
compile surface requires build coordination.

On success, after the manager confirms the packet, exact selection, and
selected-design handoff are complete, the coordinator implementer removes
exactly the manager-supplied `Temp/InterfacePrototypes/<run-id>/` scope and
records a residue proof showing that the run directory no longer exists and
that no candidate path was tracked. On worker, builder, selection, handoff, or
cleanup failure, the coordinator attempts removal of each recorded exact
target, reports every remaining path, and preserves a failure receipt. The
manager blocks continuation and routes an explicit cleanup action for any
remaining residue. No broad scratch-root cleanup or silent reuse is allowed.

## Critical files and surfaces

- `.agents/skills/external-design-interface/SKILL.md` — current read-only seam
  and future opt-in boundary.
- `.agents/skills/compile/SKILL.md` — repository-supported builder contract.
- The future `/compile`/WorktreeCli/project-target surfaces that add the
  opt-in compile-only interface-prototype target, exact candidate translation
  unit input, and candidate-local output/result.
- Coordinator implementer mutation surface — the manager-supplied ignored run
  scope for lifecycle directories, the manifest, the comparison packet, the
  selected-design handoff, and exact cleanup; it does not own candidate source.
- `AGENTS.md` — manager ownership, workflow tiering, and mutation boundaries.
- `Documents/Features/AGENTS.md` — manual Feature lifecycle.
- `Temp/InterfacePrototypes/<run-id>/` — ignored run, ownership, packet, and
  residue-proof surface; it is not tracked implementation source.

## In scope

- A separately approved workflow mode with one uniquely owned ignored run and
  one candidate subdirectory per proposal.
- Candidate-implementer-owned scratch source, coordinator-owned lifecycle
  materialization and exact cleanup under manager-supplied scope, manager-owned
  comparison semantics, user interaction, user selection, and routing, and
  builder-owned compilation and compiler output/result through the supported
  repository route.
- Exact success cleanup, failure fallback, residue proof, and selected-design
  handoff into a later implementation plan.

## Out of scope

- Changing the current Option 1 read-only skill behavior.
- Option 3 isolated prototype worktrees or branches; that design is owned by
  `IsolatedInterfacePrototypeWorktrees.md`.
- Tracked prototype source, Git branches, Git worktrees, primary landing, or
  adopting scratch output as implementation code.
- New engine interfaces, standalone compiler commands outside the delegated
  `/compile`/WorktreeCli target, unrelated build/bootstrap policy, external
  services, or credentials.

## Risk tier and invariants

Future implementation is Tier 3 because authoritative compilation adds build
coordination and the mode changes workflow mutation, evidence, and cleanup
behavior. The implementation must preserve these invariants:

- The manager owns comparison semantics, user interaction, exact user choice,
  selected-design handoff, and routing. The coordinator implementer
  materializes the run manifest, comparison packet, and selected-design
  handoff, and performs exact cleanup only within manager-supplied scope; each
  candidate implementer writes only scratch source within its assigned
  directory; compiler output/result remains builder-owned.
- All prototype source and output remain under the uniquely identified ignored
  run. Tracked paths, primary state, and worktrees stay untouched.
- The builder is the sole compiler owner, invokes the opt-in target through
  `/compile`/WorktreeCli with the exact candidate translation unit, and keeps
  each result bound to its candidate and baseline. The target links no
  executable, writes output/result only under that candidate directory, and
  shared interfaces run both Client and Server affinities.
- Coordinator cleanup targets only the manager-supplied recorded run directory.
  Success has a no-residue proof; failure reports exact residue and the
  manager blocks continuation.
- Implementation begins only after the exact user selection and its evidence
  are present in the plan. Option 3 remains excluded.

## Acceptance criteria

- A coordinator implementer creates three disjoint scratch directories below
  one manager-supplied unique ignored run and materializes the manifest with
  ownership and baseline before candidate dispatch.
- Candidate workers cannot create tracked files, worktrees, or files outside
  their assigned directory; the resulting tracked-path check proves no
  prototype source was added.
- Every compile is builder-owned through the opt-in `/compile`/WorktreeCli
  target, uses the exact candidate translation unit under its assigned
  directory, links no executable, and returns an auditable result tied to the
  candidate and fixed baseline. Candidate affinity selects Client or Server;
  shared interfaces run both affinities.
- The coordinator-materialized packet contains the manager-owned semantic
  comparison evidence for each candidate's declaration, usage,
  callers/invariants, trade-offs, compile/static observations, and exact user
  selection; the coordinator-materialized selected-design handoff is present
  in the plan.
- After manager confirmation, the coordinator removes exactly the recorded run
  directory and records a passing residue proof. A failed cleanup reports each
  remaining exact target and does not proceed to implementation or broad
  cleanup.
- No primary branch, landing state, or implementation worktree changes during
  the prototype run.

## Notes

This is a manual Feature, not an executable Plan. Its future implementation
requires a separately approved Tier-3 workflow/tooling stage because build
coordination is authoritative. It explicitly excludes the isolated-worktree
alternative and does not authorize any current-session mutation.
