# Plan Alternatives Worker

The steps each dispatched `researcher` follows. Triggers, inputs, the handoff,
the comparison, and the user gate live in [`../SKILL.md`](../SKILL.md); main's
dispatch recipe lives in root `AGENTS.md`.

## Steps

1. Read the brief text and the evidence paths it cites. Treat only the brief's
   `Fixed decisions` as binding; candidate-zero behavior is a benchmark, not an
   implicit requirement. Done when the objective, scope, fixed decisions, and
   candidate zero are each recorded separately.
2. Search the repository for a mechanism on the assigned axis, which is one of:

   - Reuse — an existing repository mechanism or pattern that already solves
     this shape, or a restructure of existing data or ownership that makes the
     problem disappear.
   - Remove the need — fix at the origin instead of the symptom, delete the
     requirement, narrow the scope, or do nothing; doing nothing is itself a
     candidate on this axis.
   - Reshape — the same objective in a different place: another layer, frame
     phase, executable, or data layout.

   Before proposing new state, trace the relevant existing ordering, ownership,
   and failure guarantees. Done when the search has produced one mechanism or the
   evidence that the axis offers none, and those guarantees have been identified.
3. Name the candidate's concrete mechanism and the 3-5 critical files it
   changes. Done when both are named, or the candidate is dropped as unnameable.
4. State what the candidate adds, what it deletes, which invariant surfaces and
   existing guarantees it uses, its new identity/lifecycle/reset/recovery state and
   verification-only machinery, and when it would be the right call. Done when each
   handoff row is stated, using `none` where the cost or guarantee is absent.
5. Return the `../SKILL.md` `## Handoff` extension fields followed by the shared
   handoff lines, or `no viable candidate on this axis` in `Candidate` with the
   reason in `Mechanism`. Done when the handoff carries every declared field.

## Rules

- Each researcher reads and searches the repository only: it writes no file, runs
  no build, and dispatches no worker.
- Each researcher returns once. There is no second round, so it decides from the
  brief and the repository rather than asking main for more.
- Each researcher works blind, from its own brief alone, and never sees another
  researcher's candidate.
- A file under `Documents/Plans/` is never an evidence path. Leave one unread
  whether the brief names it or a search surfaces it, and work from the scope
  the brief states.
- Keep every candidate on the assigned axis; leave a strong idea belonging to
  another axis out.
- Drop a candidate that cannot name its mechanism or its files, and report the
  axis as offering none, rather than returning it softened.
- Treat candidate zero as the benchmark to beat, never as a candidate to return.
