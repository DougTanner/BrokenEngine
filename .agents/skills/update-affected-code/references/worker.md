# Update Affected Code — Worker

## Steps

1. Read every owned changed region in full context and the producers,
   consumers, callers, sibling implementations, shared headers, and governing
   instructions needed to understand each handed-off contract. The C++
   conventions the propagated edits must keep are in
   [`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md).
   Done when every handed-off contract has been read at its definition and at
   the sites that depend on it.
2. Search both the old and new side of every contract. Account for all old-name
   hits after a rename and all users of the new symbol or representation. Use
   repository-tracked searches that work on the current host (prefer `git grep`
   with explicit pathspecs), limited to relevant C++, headers, and GLSL. As an
   exception to that limit and to step 6, a rename also runs `git grep` of the
   old name under `Documents/Plans` and updates quoted text in unclaimed Plans
   and their support files. Exclude `ThirdParty/`, generated data, and
   build/output directories; inspect owned
   untracked source additions directly. Do not use a moving merge base or
   `git status` alone to attribute changes. Done when both the old-side and
   new-side searches have been run and every hit is classified as needing an
   edit or not.
3. Trace signatures, defaults, units, ranges, coordinate/W conventions,
   ownership, phases, enum values, and string/table keys through every caller,
   switch, dispatch table, format string, and mirror. Preserve deliberate
   client/server and per-collection parallel structure. Done when each traced
   element has a verdict at every caller, switch, dispatch table, format
   string, and mirror it reaches.
4. Search CPU-to-GLSL and GLSL-to-CPU in both directions. Compare member order,
   byte size, alignment/padding, descriptor set and binding numbers, push
   constant ranges, enum/flag numeric values, upload/fill sites, and every
   pipeline or shader consumer. Also trace serialization order, CRC membership,
   version gates, save/replay and wire readers/writers, payload sizing, and
   numeric or table identities. A compiling site is not evidence that its old
   assumption remains valid. Done when both directions have been searched and
   every layout, binding, serialization, and identity item listed here has a
   verdict.
5. If a `Collection<T>` member or layout is added, removed, reordered, or
   retyped, read `add-collection-member`
   (`../../add-collection-member/references/worker.md`)
   completely and treat its live-variant checklist as authoritative, and run
   the collection-layout auditor from the repository root as
   `pwsh -NoProfile -File .agents/scripts/Invoke-CollectionLayoutCheck.ps1`.
   Its sweeps, shell-specific invocation, exit codes, truncation, JSON shape,
   and blocking rule are in `../../../references/collection-layout-auditor.md`.
   Report an unresolved CRC, persistence, transfer, hydration, version, or
   identity choice instead of inventing intent. Done when the auditor's typed
   result has been read and each checklist item is satisfied or reported, or
   when no `Collection<T>` member or layout changed.
6. Edit only sites whose correctness clearly depends on the new contract.
   Leave sibling features and design-dependent counterparts as residuals. Do
   not perform style fixes, documentation updates, project membership edits,
   abstractions, or incidental cleanup. Done when every dependent site is
   edited and every deliberately unedited counterpart is recorded as a
   residual.
7. Re-run the old/new searches after editing and re-read every changed region.
   Run focused static or schema checks available in context. Return compilation,
   runtime checks, domain review, and documentation sync to the manager. Done
   when the re-run searches leave no unhandled hit and those returns are in the
   handoff.
8. When no inventory receipt is supplied, create `Temp/` if absent in its own
   call, run below (pass `-Head <commit>` for a committed head and
   `-IncludeUntracked` for owned untracked working-tree files), and require a
   successful CLI exit. Done when a receipt is in hand:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot '<root>' -Baseline <full baseline Git SHA> -OutputPath Temp/update-affected-code-inventory.json
   ```

9. Read `triggers.vcxprojCandidates` from the receipt and emit an
   `/update-vcxproj` handoff for its owned rows (path and reason).
   `/update-vcxproj` owns membership, filters, and exceptions; never open or
   edit project XML. Done when emitted and no XML was opened.

## Rules

- Run as one `implementer` over a fixed owned change set.
- Propagate only edits forced by the approved behavior.
- Do not delegate, refactor, clean up, update AGENTS.md, or edit project XML.
