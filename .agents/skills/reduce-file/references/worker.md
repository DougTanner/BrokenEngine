# Reduce File Worker

The qualification, boundary-analysis, and execution steps, and the judgment
rules the runner applies. Triggers, the mode input, and the returned formats
live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Measure the target:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path <path>
   ```

   Measure several files in one batched run instead of one call per file. Use
   `-Command`, not `-File`: under `-File` the comma-separated list binds as one
   filename and the run fails.

   ```powershell
   pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'a','b','c' -Json"
   ```

   `bt-token-v1` is normalized UTF-8 bytes divided by four, rounded up. Done
   when every target has a measurement.
2. Qualify each measured target against its threshold:

   - `.h`: reduction threshold is over 5,000.
   - `.cpp`: reduction threshold is over 10,000.

   At or below the applicable threshold, report the measurement and return no
   reduction plan. Reject other extensions. Done when each target is above its
   threshold, reported at or below it, or rejected.
3. Record the original size of every measured target. Done when each target's
   original size is recorded.
4. Read applicable `AGENTS.md` files, the full target, its declaration or
   implementation counterpart, direct callers and dependencies, nearby helpers,
   and project membership. Done when each of those has been read.
5. Map:

   - declarations and definitions with line ranges and approximate sizes;
   - preprocessor affinity, templates, inline code, anonymous-namespace items,
     constants, local types, and global definitions;
   - responsibility groups, call chains, and data each group reads or writes;
   - symbols shared across proposed boundaries and include/circular-dependency
     consequences.

   Done when each of those four is mapped.
6. In approved-plan execution, use this map only to verify the approved boundary
   and discover affected sites; do not choose a different design. Done when the
   approved boundary is verified.
7. Otherwise choose the least disruptive cohesive reduction:

   1. Move independent free functions, constants, or local types into an
      existing suitable utility pair, or a new `*Utils.h` / `*Utils.cpp` pair.
   2. Extract a cohesive stateful responsibility into a new class. A concrete,
      non-template class keeps one `.h` / `.cpp` pair. The original object owns
      it by value unless a concrete lifetime, polymorphism, ABI, or dependency
      reason requires a pointer. Never introduce global ownership.
   3. Split implementations by responsibility only when the declaration is a
      static-method struct. Keep its declarations in one header.

   Done when one of those three reductions is chosen.
8. In approved execution, do steps 8-13: make only the approved moves and
   required caller/include propagation. Done when the approved moves are
   complete.
9. For every added or removed `.cpp` or `.h`, return the project/filter
   membership change as an `/update-vcxproj` trigger; do not hand-edit project
   XML. Done when every membership change is present in the handoff.
10. Return exact `Build required` rows for every affected target, naming
    configuration/platform and each selected project-member `.cpp`; for changed
    headers, name every consuming target. Return each runtime-observable
    acceptance criterion as a runtime request naming setup, action, observation,
    and required evidence. Main schedules `/compile` and runtime verification at
    their owning stages. Done when those requests are complete, or none apply.
11. Remeasure every changed and new C++ file and record each resulting size.
    Done when each has a recorded fresh measurement.
12. Run focused static checks. Done when they have run.
13. Report any file still above its applicable threshold. Done when every such
    file is reported.

## Rules

- Never add a source marker declaring an oversized file accepted.
- Never distribute one concrete class's member definitions across sibling `.cpp`
  files merely to reduce the measured file. Template definitions remain inline
  in headers unless an existing explicit-instantiation design proves otherwise.
- Prefer an existing suitable helper over a new abstraction. A proposed class
  must own meaningful data and behavior; do not wrap stateless functions in a
  class. Preserve narrow client/server guards and current public interfaces
  unless the approved design requires a change.
