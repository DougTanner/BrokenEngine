# Implement Plan — worker

The steps and rules the dispatched `implementer` follows. The public half of
this skill — purpose, triggers, brief fields, and the handoff form — is
[`../SKILL.md`](../SKILL.md).

## Steps

### Phase 1: Implement

Skip this phase in audit-only mode.

1. Confirm the execution card, scope, decisions, and acceptance checks remain
   current, and read the assigned plan items, applicable `AGENTS.md` files,
   current implementation, dependencies, helpers, and mirrored patterns before
   editing. For C++ changes, the conventions to apply are in
   [`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md).
   - Done when every listed source has been read and each remains current.
2. Before introducing a new function, type, helper, or substantial logic block,
   search with `rg` for the proposed identifier, the responsibility terms
   describing the behavior, the closest sibling implementation, and direct
   callers or consumers, then read plausible matches.
   - Done when an existing mechanism satisfying the current contract has been
     reused, or the concrete mismatch of each rejected candidate is established
     in the implementation reasoning.
3. Implement the smallest complete assigned change, preserving all snapshotted
   pre-existing work and leaving unrelated cleanup alone.
   - Done when every assigned item is implemented or stopped under step 4, and
     each unit of an approved repeated sweep or migration has passed the
     focused check its plan defines before any unit depending on it starts.
4. Stop any item where plan and repository reality meaningfully conflict, and
   quote the plan assumption and the repository evidence as a residual. Done
   when each conflicting item is stopped with its residual recorded and the
   independent valid items continue.
5. Apply compatible specialist instructions in this same context. Done when
   each applicable specialist instruction is applied, or its requirement for
   another role is recorded for return to the manager.
6. Run only the focused reads, searches, traces, and self-checks needed for
   internal coherence. The Run targeted pre-review checks stage owns the full
   applicable static pass after propagation. Done when the focused checks pass
   and every full static pass, compilation, runtime check, harness item, and
   independent review is recorded as a manager handoff.
7. Record each changed file and exact function/type/section, and for code emit
   an affected-site note naming the symbol/pattern and search scope for each
   signature, identity, semantics, layout, client/server guard, missed-caller,
   or mirrored-code concern.
   - Done when the handoff carries those rows and `Propagation required` names
     `/update-affected-code` whenever code changed, even when the note is
     `none found`.
8. For ignored or non-worktree state, follow its owner contract. Done when the
   exact path, persistence mechanism, expected persistence, and result are
   reported.

### Phase 2: Same-Context Audit

9. List zero to seven correctness or requirement risks, ranked by severity,
   each stating a Claim that names the possible failure, a Check that names a
   read/search/trace/static check, and a Result that states the evidence. Done
   when the ranked list is complete or no risk qualifies.
10. Cover the relevant dimensions without manufacturing doubts. Done when each
    of these has been considered:
    - calling context, threading, lifetime, ranges, phases, reachability,
      determinism, layout, and ownership;
    - unread callers, callees, producers, and consumers, and symbols recalled
      rather than read;
    - the current file reread after edits;
    - plan and scope attribution against the session baseline and ownership
      snapshot;
    - partial sweeps and mirrored client/server or collection wiring;
    - the smallest plausible crash, corruption, or desync path.
11. Investigate every inline-checkable item and fix confirmed in-scope
    problems. Done when each such item's check has been repeated after its fix.
12. Hand off anything requiring compilation, runtime behavior, hardware, user
    knowledge, or an independent role. Done when each of those items appears in
    the handoff.
13. Re-run any focused check whose inputs the audit changed. Evidence whose
    inputs are unchanged may be reused. Done when those checks pass, every
    audit fix is reported under `Self-audit resolved`, and the full applicable
    static pass remains assigned after propagation as
    [`../../../references/static-checks.md`](../../../references/static-checks.md)
    assigns them.

## Rules

- Workers never delegate; return any separate-role work to the manager.
- Do not infer a missing session baseline or ownership snapshot from a moving
  merge base or the current status. Return the missing input to the manager.
- In audit-only mode, continue only in the context that made the named changes;
  derive its touched regions from edit history and the session-baseline diff
  while excluding the ownership snapshot.
- Include a rejected reuse candidate in the handoff only when that candidate or
  the resulting duplication is non-obvious to a reviewer. Repository facts
  remain resolvable through continued read-only inspection.
- If ownership, invariant exposure, or the spread of breakage in shared code
  remains meaningfully uncertain after the targeted searches, do not edit the
  affected shared region; return the exact unknown as a residual for manager
  resolution, with user involvement when necessary.
- Never improvise a replacement design for a plan assumption the repository
  contradicts.
- A repeated sweep's focused checkpoints create no per-unit commit or stage and
  never replace the stage's final acceptance; a failed checkpoint stops the
  dependent work and returns the evidence to the manager. Builder, runtime, and
  independent reviewer checks stay manager-owned handoffs.
- The worker that made the changes performs the audit phase before returning.
  It never substitutes for domain, adversarial, or session review.
- Audit fixes remain inside the assignment, and never claim that compilation or
  runtime verification passed.
