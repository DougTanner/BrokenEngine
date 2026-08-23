# Interface prototype discriminator

## Question

For the interface-prototyping workflow, who owns mutations, how are scratch
artifacts created and cleaned, what evidence survives for comparison, and where
is the Tier boundary between design exploration and implementation?

## Current evidence

`external-design-interface` asks for three radically different C++ interface
designs, a comparison, and a user choice before implementation. The general
Change Workflow requires the manager to own scope, approval, and landing, while
workers must preserve unrelated work. The current contracts do not settle
whether prototypes may mutate the shared checkout, which scratch paths are
retained, or how a chosen design becomes a decision-complete Plan.

## Options to compare

1. Read-only proposals in the manager context, with no scratch source. This
   maximizes safety but may miss compile/layout evidence.
2. Worker-owned scratch artifacts outside tracked paths, with a manager-owned
   comparison packet. This allows richer prototypes but requires cleanup and
   evidence retention rules.
3. Isolated prototype worktrees/branches with explicit disposal and a selected
   design handoff. This gives the strongest implementation evidence but adds
   worktree and landing coordination.

## Evidence to collect

- Mutation authority for each option: who may create, edit, compile, or remove
  scratch artifacts, and which actions remain manager-only.
- Scratch lifecycle: path class, ownership, retention window, cleanup trigger,
  failure cleanup, and proof that no ignored or tracked residue is mistaken for
  implementation work.
- Comparison evidence: interface shape, affected callers, invariants, compile
  or static observations, and the exact user decision that selects one option.
- Tier boundary: when a prototype touches public API, data layout, threading,
  determinism/CRC, wire, build/bootstrap, or another integration surface and
  therefore requires a Tier 2 or Tier 3 Plan.

## Promotion criteria

Promote to an implementation Plan only after mutation authority, scratch
lifecycle/cleanup, evidence retention, user selection, and tier classification
are explicit. Promote to a workflow/tooling Plan separately if the chosen
prototype mechanism changes delegation or landing behavior. A proposal that
still leaves scratch ownership or cleanup open remains an investigation.

## Non-goals

- No C++ interface, scratch script, worktree, or cleanup implementation is
  authorized here.
- No user choice is inferred from a prototype's apparent quality, and no
  external service or credential is used.

## Notes

The three designs are alternatives for the workflow's mutation/evidence seam,
not three implementation APIs to be silently merged. The user-selected design
and its evidence must be preserved before implementation begins.
