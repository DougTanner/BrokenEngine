# Design Interface Worker

The brief, research, design, and synthesis steps, plus the judgment rules they
assume. Triggers, inputs, and the comparison the manager presents live in
[`../SKILL.md`](../SKILL.md).

## Steps

### Establish the Design Brief

1. Inspect the repository before asking about gaps. Record:

   - problem, callers, operations, ownership, and existing interfaces;
   - allocation tracking, SOA layout, threading, build affinity, and frame
     phases;
   - bit-deterministic, CRC-checked PostRender state versus non-deterministic
     Interpolate/render state and client-only visuals.

   Done when each recorded item is in the brief.
2. Sort the recorded material into essential invariants the repository actually
   requires and accidental seams a current caller happens to expose, such as its
   parameter shape or call order; a design may discard an accidental seam, so
   never carry one forward as if it were a contract. Done when every recorded
   item is sorted.
3. Keep passing the scope and compatibility constraints into every design. Done
   when those constraints reach each design.
4. Before any delegation, decide whether the requested system or a viable design
   could add a `Collection<T>` type or SOA member pointer. Done when that
   decision is made.
5. When it could, read `/add-collection` and `/add-collection-member` and pass
   their applicable layout, lifecycle, persistence, CRC, version, and
   registration constraints to every worker. Done when any applicable
   constraints are attached.

### Research Existing Patterns

6. Dispatch one `researcher` from the main context before any other worker.
   Done when it is running.
7. Give it the brief, the exact repository scope, and the pattern list to
   identify: similar live systems, dominant caller patterns, naming and
   parameter conventions, manager access, workbuffer usage, and relevant
   collection shapes. Done when the brief is sent.
8. Wait for its evidence before continuing. Done when the researcher extension
   and its complete shared handoff have returned with concise paths and symbols.

### Generate Three Designs

9. After research completes, dispatch exactly three `planner` workers
   concurrently from the main context. Give each worker the same brief,
   repository evidence, applicable collection constraints, and one unsoftened
   axis:

   1. Minimal surface: 1–3 entry points, opaque internals, deep module.
   2. Data locality: contiguous SOA iteration, batching, and workbuffer use.
   3. Caller ergonomics: optimize readability and simplicity for the dominant
      caller pattern found by research.

    Done when all three designs have returned.
10. Require from each worker the planner extension and its complete shared
    handoff: C++ interface declarations, representative usage, hidden
    complexity, client/server guard impact, allocation behavior, threading and
    frame-phase behavior, deterministic-state impact, and trade-offs. Done when
    every returned design carries those items and the shared fields.
11. Require each design to identify explicitly any proposed Collection or
    member. Done when every returned design names them and implements no bodies.

### Synthesize and Gate the Plan

12. After the user decides, create a complete implementation plan containing the
    final header-style interface, key call-site examples, incorporated elements,
    affected integration sites and invariants, exclusions, and decisive
    acceptance checks. Done when that plan contains each of those.
13. Classify that plan under the root `AGENTS.md` Change Workflow Approve and
    classify through Plan review steps, which own the tier and the plan-review
    gates that tier requires. Done when the plan is classified.
14. When the plan adds a Collection, record that `/add-collection` owns its
    mechanical wiring and invokes `/add-collection-member` for every SOA column.
    Done when the plan records that ownership.

## Rules

- The manager, researcher, and planners may inspect the repository, but they do
  not create, edit, or remove scratch source, build output, branches, or
  worktrees, and they do not compile candidates.
- The workflow ends at the decision and plan gate; do not begin implementation
  from this skill. Follow root `AGENTS.md` for delegation, plan, review, and
  landing mechanics rather than restating them here.
- No dispatched worker delegates further.
- Use no more than the three available child slots and do not add a
  manager/singleton-specific fourth design.
- A requirement the user states while the brief is being built becomes a core
  constraint of all three designs, not a late amendment applied afterwards.
